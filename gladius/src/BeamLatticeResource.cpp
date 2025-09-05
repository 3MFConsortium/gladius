#include "BeamLatticeResource.h"
#include "EventLogger.h"
#include "Primitives.h"

#include <algorithm>
#include <stdexcept>

namespace gladius
{
    BeamLatticeResource::BeamLatticeResource(ResourceKey key,
                                             std::vector<BeamData> && beams,
                                             std::vector<BallData> && balls)
        : ResourceBase(std::move(key))
        , m_beams(std::move(beams))
        , m_balls(std::move(balls))
    {
        // Validate input data
        if (m_beams.empty() && m_balls.empty())
        {
            throw std::invalid_argument(
              "BeamLatticeResource: Cannot create resource with no beams or balls");
        }

        // Set reasonable BVH build parameters
        m_bvhParams.maxDepth = 20;
        m_bvhParams.maxPrimitivesPerLeaf = 4;
        m_bvhParams.traversalCost = 1.0f;
        m_bvhParams.intersectionCost = 2.0f;
    }

    void BeamLatticeResource::loadImpl()
    {
        // Clear any existing data
        m_payloadData.meta.clear();
        m_payloadData.data.clear();
        m_bvhNodes.clear();

        // Build BVH hierarchy from beam and ball data
        BeamBVHBuilder builder;
        m_bvhNodes = builder.build(m_beams, m_balls, m_bvhParams);
        m_buildStats = builder.getLastBuildStats();

        // Write BVH nodes to payload data
        writeBVHNodesToPayload();

        // Write beam primitives to payload data
        writeBeamPrimitivesToPayload();

        // Write ball primitives to payload data
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

            // Write ball radius and material (2 floats)
            m_payloadData.data.push_back(ball.radius);
            m_payloadData.data.push_back(static_cast<float>(ball.materialId));
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
        // Calculate tight bounding box for conical beam
        const float maxRadius = std::max(beam.startRadius, beam.endRadius);

        BoundingBox bounds;
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
