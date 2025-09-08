#include "BeamLatticeResource.h"
#include "BeamLatticeVoxelAcceleration.h"
#include "Primitives.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace gladius
{
    BeamLatticeResource::BeamLatticeResource(ResourceKey key,
                                             std::vector<BeamData> && beams,
                                             std::vector<BallData> && balls,
                                             bool useVoxelAcceleration)
        : ResourceBase(std::move(key))
        , m_beams(std::move(beams))
        , m_balls(std::move(balls))
        , m_useVoxelAcceleration(useVoxelAcceleration)
    {
        // Validate input data
        if (m_beams.empty() && m_balls.empty())
        {
            throw std::invalid_argument(
              "BeamLatticeResource: Cannot create resource with no beams or balls");
        }

        // Set optimized BVH build parameters for large lattices
        m_bvhParams.maxDepth = 16;
        m_bvhParams.maxPrimitivesPerLeaf = 8;
        m_bvhParams.traversalCost = 1.0f;
        m_bvhParams.intersectionCost = 2.0f;
    }

    void BeamLatticeResource::loadImpl()
    {
        // Clear any existing data
        m_payloadData.meta.clear();
        m_payloadData.data.clear();
        m_bvhNodes.clear();

        // Build acceleration structure based on configuration
        buildAccelerationStructure();
    }

    void BeamLatticeResource::buildAccelerationStructure()
    {
        if (m_useVoxelAcceleration)
        {
            buildVoxelAcceleration();
        }
        else
        {
            buildBVH();
        }
    }

    void BeamLatticeResource::buildBVH()
    {
        // Build BVH hierarchy from beam and ball data
        BeamBVHBuilder builder;
        m_bvhNodes = builder.build(m_beams, m_balls, m_bvhParams);
        m_buildStats = builder.getLastBuildStats();

        // Write BVH nodes to payload data
        writeBVHNodesToPayload();

        // Write primitive indices mapping to payload data
        writePrimitiveIndicesToPayload(builder.getPrimitiveOrdering());

        // Write beam primitives to payload data
        writeBeamPrimitivesToPayload();

        // Write ball primitives to payload data
        writeBallPrimitivesToPayload();
    }

    void BeamLatticeResource::buildVoxelAcceleration()
    {
        // Build voxel grids from beam/ball data
        BeamLatticeVoxelSettings settings;
        settings.voxelSize = 0.5f; // TODO: expose as parameter
        settings.maxDistance = 10.0f;
        settings.separateBeamBallGrids = false;
        settings.encodeTypeInIndex = true; // encode type in upper bit
        settings.enableDebugOutput = false;

        BeamLatticeVoxelBuilder builder;
        auto [primitiveIndexGrid, primitiveTypeGrid] =
          builder.buildVoxelGrids(m_beams, m_balls, settings);

        if (!primitiveIndexGrid)
        {
            // Fallback to BVH if voxel build failed
            buildBVH();
            return;
        }

        // Write a Beam Lattice meta so kernel knows to look for voxel grid next
        PrimitiveMeta latticeMeta = {};
        latticeMeta.primitiveType = SDF_BEAM_LATTICE;
        latticeMeta.start = static_cast<int>(m_payloadData.data.size());
        latticeMeta.end = latticeMeta.start; // no direct data for the lattice node
        latticeMeta.scaling = 1.0f;
        m_payloadData.meta.push_back(latticeMeta);

        // Serialize voxel grid header and data as simple flat buffer expected by kernel
        // Header layout: origin (3), dimensions (3), voxel size (1), padding (2) => we use 9 floats
        // total
        auto const & bbox = primitiveIndexGrid->evalActiveVoxelBoundingBox();
        auto const & transform = primitiveIndexGrid->transform();
        openvdb::Vec3d originIndex(bbox.min().x(), bbox.min().y(), bbox.min().z());
        openvdb::Vec3d originWorld = transform.indexToWorld(originIndex);
        openvdb::Coord dim = bbox.dim();

        PrimitiveMeta voxelMeta = {};
        voxelMeta.primitiveType = SDF_BEAM_LATTICE_VOXEL_INDEX;
        voxelMeta.start = static_cast<int>(m_payloadData.data.size());

        // origin (world)
        m_payloadData.data.push_back(static_cast<float>(originWorld.x()));
        m_payloadData.data.push_back(static_cast<float>(originWorld.y()));
        m_payloadData.data.push_back(static_cast<float>(originWorld.z()));
        // dimensions
        m_payloadData.data.push_back(static_cast<float>(dim.x()));
        m_payloadData.data.push_back(static_cast<float>(dim.y()));
        m_payloadData.data.push_back(static_cast<float>(dim.z()));
        // voxel size
        float const voxelSize = static_cast<float>(transform.voxelSize()[0]);
        m_payloadData.data.push_back(voxelSize);
        // two padding values to reach 9 floats like kernel expects
        m_payloadData.data.push_back(0.0f);
        m_payloadData.data.push_back(0.0f);

        // Flat voxel data in z-major order used by kernel: z * Y * X + y * X + x
        auto accessor = primitiveIndexGrid->getAccessor();
        for (int z = bbox.min().z(); z <= bbox.max().z(); ++z)
        {
            for (int y = bbox.min().y(); y <= bbox.max().y(); ++y)
            {
                for (int x = bbox.min().x(); x <= bbox.max().x(); ++x)
                {
                    openvdb::Coord c(x, y, z);
                    int v = accessor.getValue(c);
                    m_payloadData.data.push_back(static_cast<float>(v));
                }
            }
        }

        voxelMeta.end = static_cast<int>(m_payloadData.data.size());
        voxelMeta.scaling = 1.0f;
        m_payloadData.meta.push_back(voxelMeta);

        // Add buffer alignment padding to prevent memory corruption between NanoVDB grids and
        // beam/ball data NanoVDB requires 32-byte alignment, so we ensure the next data region
        // starts at a proper boundary
        constexpr size_t BUFFER_ALIGNMENT = 32; // Match CNANOVDB_DATA_ALIGNMENT
        size_t currentByteOffset = m_payloadData.data.size() * sizeof(float);
        size_t paddingBytes =
          (BUFFER_ALIGNMENT - (currentByteOffset % BUFFER_ALIGNMENT)) % BUFFER_ALIGNMENT;
        size_t paddingFloats =
          (paddingBytes + sizeof(float) - 1) / sizeof(float); // Round up to float boundary

        // Add padding floats to ensure alignment
        for (size_t i = 0; i < paddingFloats; ++i)
        {
            m_payloadData.data.push_back(0.0f);
        }

        // Primitive indices mapping is still needed for evaluation details (BVH path fallbacks)
        // We create the same ordering as BVH builder would: beams first then balls
        std::vector<BeamPrimitive> ordering;
        ordering.reserve(m_beams.size() + m_balls.size());
        for (size_t i = 0; i < m_beams.size(); ++i)
        {
            ordering.emplace_back(
              BeamPrimitive::BEAM, static_cast<int>(i), calculateBeamBounds(m_beams[i]));
        }
        for (size_t i = 0; i < m_balls.size(); ++i)
        {
            ordering.emplace_back(
              BeamPrimitive::BALL, static_cast<int>(i), calculateBallBounds(m_balls[i]));
        }
        writePrimitiveIndicesToPayload(ordering);

        // Write beam and ball raw data blocks for distance evaluation
        writeBeamPrimitivesToPayload();
        writeBallPrimitivesToPayload();
    }

    void BeamLatticeResource::write(Primitives & primitives)
    {
        // Add our payload data to primitives - the ResourceManager handles load state checking
        m_startIndex = static_cast<int>(primitives.primitives.getSize());
        primitives.add(m_payloadData);
        m_endIndex = static_cast<int>(primitives.primitives.getSize());
    }

    void BeamLatticeResource::writeBVHNodesToPayload()
    {
        // For now, let's write a single top-level metadata entry for the beam lattice
        PrimitiveMeta meta = {};
        meta.primitiveType = SDF_BEAM_LATTICE;
        meta.start = static_cast<int>(m_payloadData.data.size());

        // Calculate overall bounding box
        if (!m_bvhNodes.empty())
        {
            meta.boundingBox = m_bvhNodes[0].boundingBox; // Root node bounds
            meta.center = {(meta.boundingBox.min.x + meta.boundingBox.max.x) * 0.5f,
                           (meta.boundingBox.min.y + meta.boundingBox.max.y) * 0.5f,
                           (meta.boundingBox.min.z + meta.boundingBox.max.z) * 0.5f,
                           0.0f};
        }

        meta.scaling = 1.0f;

        // Write BVH node data to payload
        for (const auto & node : m_bvhNodes)
        {
            // Pack bounding box
            m_payloadData.data.push_back(node.boundingBox.min.x);
            m_payloadData.data.push_back(node.boundingBox.min.y);
            m_payloadData.data.push_back(node.boundingBox.min.z);
            m_payloadData.data.push_back(node.boundingBox.max.x);
            m_payloadData.data.push_back(node.boundingBox.max.y);
            m_payloadData.data.push_back(node.boundingBox.max.z);

            // Pack node metadata as floats
            m_payloadData.data.push_back(static_cast<float>(node.leftChild));
            m_payloadData.data.push_back(static_cast<float>(node.rightChild));
            m_payloadData.data.push_back(static_cast<float>(node.primitiveStart));
            m_payloadData.data.push_back(static_cast<float>(node.primitiveCount));
        }

        meta.end = static_cast<int>(m_payloadData.data.size());
        m_payloadData.meta.push_back(meta);
    }

    void BeamLatticeResource::writePrimitiveIndicesToPayload(
      const std::vector<BeamPrimitive> & primitiveOrdering)
    {
        if (primitiveOrdering.empty())
        {
            return;
        }

        PrimitiveMeta meta = {};
        meta.primitiveType = SDF_PRIMITIVE_INDICES; // We'll need to define this type
        meta.start = static_cast<int>(m_payloadData.data.size());

        // Write primitive mapping data
        // Each primitive entry has 3 floats: type (beam=0, ball=1), index, unused
        for (const auto & primitive : primitiveOrdering)
        {
            m_payloadData.data.push_back(static_cast<float>(primitive.type)); // 0 = BEAM, 1 = BALL
            m_payloadData.data.push_back(
              static_cast<float>(primitive.index)); // Index into beam/ball array
            m_payloadData.data.push_back(0.0f); // Unused - pad to align with other data structures
        }

        meta.end = static_cast<int>(m_payloadData.data.size());
        m_payloadData.meta.push_back(meta);
    }

    void BeamLatticeResource::writeBeamPrimitivesToPayload()
    {
        if (m_beams.empty())
        {
            return;
        }

        PrimitiveMeta meta = {};
        meta.primitiveType = SDF_BEAM;
        meta.start = static_cast<int>(m_payloadData.data.size());

        // Write beam data directly to payload data vector
        for (const auto & beam : m_beams)
        {
            // Write beam start position (3 floats)
            m_payloadData.data.push_back(beam.startPos.x);
            m_payloadData.data.push_back(beam.startPos.y);
            m_payloadData.data.push_back(beam.startPos.z);

            // Write beam end position (3 floats)
            m_payloadData.data.push_back(beam.endPos.x);
            m_payloadData.data.push_back(beam.endPos.y);
            m_payloadData.data.push_back(beam.endPos.z);

            // Write radii (2 floats)
            m_payloadData.data.push_back(beam.startRadius);
            m_payloadData.data.push_back(beam.endRadius);

            // Write material and cap info (3 floats)
            m_payloadData.data.push_back(static_cast<float>(beam.startCapStyle));
            m_payloadData.data.push_back(static_cast<float>(beam.endCapStyle));
            m_payloadData.data.push_back(static_cast<float>(beam.materialId));
        }

        meta.end = static_cast<int>(m_payloadData.data.size());
        meta.scaling = 1.0f;

        // Calculate overall bounding box for all beams
        if (!m_beams.empty())
        {
            meta.boundingBox = calculateBeamBounds(m_beams[0]);
            for (size_t i = 1; i < m_beams.size(); ++i)
            {
                BoundingBox beamBounds = calculateBeamBounds(m_beams[i]);
                // Expand overall bounds
                meta.boundingBox.min.x = std::min(meta.boundingBox.min.x, beamBounds.min.x);
                meta.boundingBox.min.y = std::min(meta.boundingBox.min.y, beamBounds.min.y);
                meta.boundingBox.min.z = std::min(meta.boundingBox.min.z, beamBounds.min.z);
                meta.boundingBox.max.x = std::max(meta.boundingBox.max.x, beamBounds.max.x);
                meta.boundingBox.max.y = std::max(meta.boundingBox.max.y, beamBounds.max.y);
                meta.boundingBox.max.z = std::max(meta.boundingBox.max.z, beamBounds.max.z);
            }

            meta.center = {(meta.boundingBox.min.x + meta.boundingBox.max.x) * 0.5f,
                           (meta.boundingBox.min.y + meta.boundingBox.max.y) * 0.5f,
                           (meta.boundingBox.min.z + meta.boundingBox.max.z) * 0.5f,
                           0.0f};
        }

        m_payloadData.meta.push_back(meta);
    }

    void BeamLatticeResource::writeBallPrimitivesToPayload()
    {
        if (m_balls.empty())
        {
            return; // No balls to write
        }

        PrimitiveMeta meta = {};
        meta.primitiveType = SDF_BALL;
        meta.start = static_cast<int>(m_payloadData.data.size());

        // Write ball data directly to payload data vector
        for (const auto & ball : m_balls)
        {
            // Write ball center (3 floats)
            m_payloadData.data.push_back(ball.position.x);
            m_payloadData.data.push_back(ball.position.y);
            m_payloadData.data.push_back(ball.position.z);

            // Write ball radius (1 float)
            m_payloadData.data.push_back(ball.radius);
        }

        meta.end = static_cast<int>(m_payloadData.data.size());
        meta.scaling = 1.0f;

        // Calculate overall bounding box for all balls
        if (!m_balls.empty())
        {
            meta.boundingBox = calculateBallBounds(m_balls[0]);
            for (size_t i = 1; i < m_balls.size(); ++i)
            {
                BoundingBox ballBounds = calculateBallBounds(m_balls[i]);
                // Expand overall bounds
                meta.boundingBox.min.x = std::min(meta.boundingBox.min.x, ballBounds.min.x);
                meta.boundingBox.min.y = std::min(meta.boundingBox.min.y, ballBounds.min.y);
                meta.boundingBox.min.z = std::min(meta.boundingBox.min.z, ballBounds.min.z);
                meta.boundingBox.max.x = std::max(meta.boundingBox.max.x, ballBounds.max.x);
                meta.boundingBox.max.y = std::max(meta.boundingBox.max.y, ballBounds.max.y);
                meta.boundingBox.max.z = std::max(meta.boundingBox.max.z, ballBounds.max.z);
            }

            meta.center = {(meta.boundingBox.min.x + meta.boundingBox.max.x) * 0.5f,
                           (meta.boundingBox.min.y + meta.boundingBox.max.y) * 0.5f,
                           (meta.boundingBox.min.z + meta.boundingBox.max.z) * 0.5f,
                           0.0f};
        }

        m_payloadData.meta.push_back(meta);
    }

    BoundingBox BeamLatticeResource::calculateBeamBounds(const BeamData & beam)
    {
        BoundingBox bounds;
        float maxRadius = std::max(beam.startRadius, beam.endRadius);
        bounds.min.x = std::min(beam.startPos.x, beam.endPos.x) - maxRadius;
        bounds.min.y = std::min(beam.startPos.y, beam.endPos.y) - maxRadius;
        bounds.min.z = std::min(beam.startPos.z, beam.endPos.z) - maxRadius;
        bounds.min.w = 0.0f;

        bounds.max.x = std::max(beam.startPos.x, beam.endPos.x) + maxRadius;
        bounds.max.y = std::max(beam.startPos.y, beam.endPos.y) + maxRadius;
        bounds.max.z = std::max(beam.startPos.z, beam.endPos.z) + maxRadius;
        bounds.max.w = 0.0f;

        return bounds;
    }

    BoundingBox BeamLatticeResource::calculateBallBounds(const BallData & ball)
    {
        BoundingBox bounds;
        bounds.min.x = ball.position.x - ball.radius;
        bounds.min.y = ball.position.y - ball.radius;
        bounds.min.z = ball.position.z - ball.radius;
        bounds.min.w = 0.0f;

        bounds.max.x = ball.position.x + ball.radius;
        bounds.max.y = ball.position.y + ball.radius;
        bounds.max.z = ball.position.z + ball.radius;
        bounds.max.w = 0.0f;

        return bounds;
    }
}
