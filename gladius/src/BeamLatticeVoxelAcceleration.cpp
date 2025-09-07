/**
 * @file BeamLatticeVoxelAcceleration.cpp
 * @brief Implementation of voxel-based acceleration for beam lattices
 */

#include "BeamLatticeVoxelAcceleration.h"
#include "Primitives.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <openvdb/openvdb.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/Composite.h>

namespace gladius
{
    std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
    BeamLatticeVoxelBuilder::buildVoxelGrids(std::vector<BeamData> const & beams,
                                             std::vector<BallData> const & balls,
                                             BeamLatticeVoxelSettings const & settings)
    {
        auto startTime = std::chrono::high_resolution_clock::now();
        m_lastStats = BuildStats{};

        if (beams.empty() && balls.empty())
        {
            return {nullptr, nullptr};
        }

        // Ensure OpenVDB is initialized
        openvdb::initialize();

        // Create transform for voxel grid
        auto transform = openvdb::math::Transform::createLinearTransform(settings.voxelSize);

        // Calculate bounding box for all primitives
        openvdb::BBoxd bbox = calculateBoundingBox(beams, balls);

        // Phase 1 Optimization: Pre-compute bounding boxes for all primitives
        auto beamBounds = precomputeBeamBounds(beams);
        auto ballBounds = precomputeBallBounds(balls);

        // Create primitive index grid with background 0 (means no primitive)
        auto primitiveIndexGrid = openvdb::Int32Grid::create(0);
        primitiveIndexGrid->setTransform(transform);
        primitiveIndexGrid->setName("beam_lattice_primitive_indices");

        // Optional type grid (if not encoding type in index)
        openvdb::Int32Grid::Ptr primitiveTypeGrid = nullptr;
        if (settings.separateBeamBallGrids)
        {
            primitiveTypeGrid = openvdb::Int32Grid::create(-1); // Background = no type
            primitiveTypeGrid->setTransform(transform);
            primitiveTypeGrid->setName("beam_lattice_primitive_types");
        }

        // Accessor for index grid; for the optional type grid we'll use the grid API directly to
        // avoid accessor lifetime issues
        auto indexAccessor = primitiveIndexGrid->getAccessor();

        // Convert bounding box to index space
        openvdb::Coord minCoord = transform->worldToIndexNodeCentered(bbox.min());
        openvdb::Coord maxCoord = transform->worldToIndexNodeCentered(bbox.max());

        // Expand by a few voxels to ensure coverage
        int const margin =
          static_cast<int>(std::ceil(settings.maxDistance / settings.voxelSize)) + 2;
        minCoord.offset(-margin);
        maxCoord.offset(margin);

        if (settings.enableDebugOutput)
        {
            openvdb::Coord gridSize = maxCoord - minCoord;
            m_lastStats.totalVoxels =
              static_cast<size_t>(gridSize.x()) * gridSize.y() * gridSize.z();

            std::cout << "BeamLatticeVoxelBuilder: Using optimized Phase 1 implementation\n";
            std::cout << "  Pre-computed " << beamBounds.size() << " beam bounds and "
                      << ballBounds.size() << " ball bounds\n";
        }

        // Process each voxel in the bounding box (triple loop; can be optimized later)
        size_t processedVoxels = 0;
        float totalDistance = 0.0f;

        for (openvdb::Coord coord(minCoord.x(), 0, 0); coord.x() <= maxCoord.x(); ++coord.x())
        {
            for (coord.y() = minCoord.y(); coord.y() <= maxCoord.y(); ++coord.y())
            {
                for (coord.z() = minCoord.z(); coord.z() <= maxCoord.z(); ++coord.z())
                {
                    // Convert voxel coordinate to world space
                    openvdb::Vec3d worldPos = transform->indexToWorld(coord);
                    openvdb::Vec3f pos(static_cast<float>(worldPos.x()),
                                       static_cast<float>(worldPos.y()),
                                       static_cast<float>(worldPos.z()));

                    // Phase 1 Optimization: Use optimized primitive search with cached bounds
                    auto const [primitiveIndex, primitiveType] = findClosestPrimitiveOptimized(
                      pos, beamBounds, ballBounds, beams, balls, settings.maxDistance);

                    if (primitiveIndex >= 0)
                    {
                        // Store primitive index
                        if (settings.encodeTypeInIndex && !settings.separateBeamBallGrids)
                        {
                            int encodedIndex = primitiveIndex;
                            if (primitiveType == 1) // ball
                            {
                                encodedIndex |= (1 << 31);
                            }
                            indexAccessor.setValue(coord, encodedIndex);
                        }
                        else
                        {
                            indexAccessor.setValue(coord, primitiveIndex);
                            if (primitiveTypeGrid)
                            {
                                // Set value in the underlying tree when type grid is enabled
                                primitiveTypeGrid->tree().setValueOn(coord, primitiveType);
                            }
                        }

                        m_lastStats.activeVoxels++;

                        // Distance for stats (approx)
                        float distance = (primitiveType == 0)
                                           ? calculateBeamDistance(pos, beams[primitiveIndex])
                                           : calculateBallDistance(pos, balls[primitiveIndex]);
                        totalDistance += std::abs(distance);
                        m_lastStats.maxDistance =
                          std::max(m_lastStats.maxDistance, std::abs(distance));
                    }

                    processedVoxels++;
                }
            }
        }

        if (m_lastStats.activeVoxels > 0)
        {
            m_lastStats.averageDistance =
              totalDistance / static_cast<float>(m_lastStats.activeVoxels);
        }

        primitiveIndexGrid->pruneGrid();
        if (primitiveTypeGrid)
        {
            primitiveTypeGrid->pruneGrid();
        }

        m_lastStats.memoryUsageBytes = primitiveIndexGrid->memUsage();
        if (primitiveTypeGrid)
        {
            m_lastStats.memoryUsageBytes += primitiveTypeGrid->memUsage();
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        m_lastStats.buildTimeSeconds = duration.count() / 1000.0f;

        return {primitiveIndexGrid, primitiveTypeGrid};
    }

    float BeamLatticeVoxelBuilder::calculateBeamDistance(openvdb::Vec3f const & point,
                                                         BeamData const & beam) const
    {
        // Convert beam endpoints to Vec3f
        openvdb::Vec3f startPos(static_cast<float>(beam.startPos.x),
                                static_cast<float>(beam.startPos.y),
                                static_cast<float>(beam.startPos.z));
        openvdb::Vec3f endPos(static_cast<float>(beam.endPos.x),
                              static_cast<float>(beam.endPos.y),
                              static_cast<float>(beam.endPos.z));

        openvdb::Vec3f axis = endPos - startPos;
        float length_val = axis.length();
        if (length_val < 1e-6f)
        {
            float radius = std::max(beam.startRadius, beam.endRadius);
            openvdb::Vec3f d = point - startPos;
            return d.length() - radius;
        }

        openvdb::Vec3f dir = axis / length_val;
        openvdb::Vec3f v = point - startPos;
        float t = v.dot(dir);
        t = std::max(0.0f, std::min(length_val, t));
        openvdb::Vec3f closest = startPos + dir * t;
        float coreDist = (point - closest).length();

        // Linear radius interpolation
        float radius = beam.startRadius + (beam.endRadius - beam.startRadius) * (t / length_val);
        return coreDist - radius;
    }

    float BeamLatticeVoxelBuilder::calculateBallDistance(openvdb::Vec3f const & point,
                                                         BallData const & ball) const
    {
        float dx = point.x() - ball.position.x;
        float dy = point.y() - ball.position.y;
        float dz = point.z() - ball.position.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz) - ball.radius;
        return dist;
    }

    std::pair<int, int>
    BeamLatticeVoxelBuilder::findClosestPrimitive(openvdb::Vec3f const & point,
                                                  std::vector<BeamData> const & beams,
                                                  std::vector<BallData> const & balls,
                                                  float maxDist) const
    {
        float bestDist = maxDist;
        int bestIndex = -1;
        int bestType = -1; // 0=beam, 1=ball

        // Phase 1 Optimization: Check beams with bounding box pre-filtering
        for (size_t i = 0; i < beams.size(); ++i)
        {
            BeamData const & beam = beams[i];

            // Quick bounding box check: compute beam AABB
            float minX =
              std::min(beam.startPos.x, beam.endPos.x) - std::max(beam.startRadius, beam.endRadius);
            float maxX =
              std::max(beam.startPos.x, beam.endPos.x) + std::max(beam.startRadius, beam.endRadius);
            float minY =
              std::min(beam.startPos.y, beam.endPos.y) - std::max(beam.startRadius, beam.endRadius);
            float maxY =
              std::max(beam.startPos.y, beam.endPos.y) + std::max(beam.startRadius, beam.endRadius);
            float minZ =
              std::min(beam.startPos.z, beam.endPos.z) - std::max(beam.startRadius, beam.endRadius);
            float maxZ =
              std::max(beam.startPos.z, beam.endPos.z) + std::max(beam.startRadius, beam.endRadius);

            // Skip if point is outside expanded bounding box by more than current best distance
            if (point.x() < minX - bestDist || point.x() > maxX + bestDist ||
                point.y() < minY - bestDist || point.y() > maxY + bestDist ||
                point.z() < minZ - bestDist || point.z() > maxZ + bestDist)
            {
                continue;
            }

            float d = calculateBeamDistance(point, beam);
            if (d < bestDist)
            {
                bestDist = d;
                bestIndex = static_cast<int>(i);
                bestType = 0;

                // Early termination: if we're inside the primitive, we found the closest
                if (d <= 0.0f)
                {
                    return {bestIndex, bestType};
                }
            }
        }

        // Phase 1 Optimization: Check balls with bounding box pre-filtering
        for (size_t i = 0; i < balls.size(); ++i)
        {
            BallData const & ball = balls[i];

            // Quick bounding box check for balls (simpler case)
            float dx = std::abs(point.x() - ball.position.x);
            float dy = std::abs(point.y() - ball.position.y);
            float dz = std::abs(point.z() - ball.position.z);

            // Skip if point is definitely too far (use Manhattan distance as cheap upper bound)
            if (dx + dy + dz > ball.radius + bestDist)
            {
                continue;
            }

            float d = calculateBallDistance(point, ball);
            if (d < bestDist)
            {
                bestDist = d;
                bestIndex = static_cast<int>(i);
                bestType = 1;

                // Early termination: if we're inside the primitive, we found the closest
                if (d <= 0.0f)
                {
                    return {bestIndex, bestType};
                }
            }
        }

        return {bestIndex, bestType};
    }

    openvdb::BBoxd
    BeamLatticeVoxelBuilder::calculateBoundingBox(std::vector<BeamData> const & beams,
                                                  std::vector<BallData> const & balls) const
    {
        openvdb::Vec3d minP(1e30, 1e30, 1e30);
        openvdb::Vec3d maxP(-1e30, -1e30, -1e30);

        auto extend = [&](float x, float y, float z)
        {
            minP.x() = std::min(minP.x(), static_cast<double>(x));
            minP.y() = std::min(minP.y(), static_cast<double>(y));
            minP.z() = std::min(minP.z(), static_cast<double>(z));
            maxP.x() = std::max(maxP.x(), static_cast<double>(x));
            maxP.y() = std::max(maxP.y(), static_cast<double>(y));
            maxP.z() = std::max(maxP.z(), static_cast<double>(z));
        };

        for (auto const & b : beams)
        {
            extend(b.startPos.x, b.startPos.y, b.startPos.z);
            extend(b.endPos.x, b.endPos.y, b.endPos.z);
        }
        for (auto const & s : balls)
        {
            extend(s.position.x, s.position.y, s.position.z);
            extend(s.position.x + s.radius, s.position.y + s.radius, s.position.z + s.radius);
            extend(s.position.x - s.radius, s.position.y - s.radius, s.position.z - s.radius);
        }

        return {minP, maxP};
    }

    std::vector<BeamLatticeVoxelBuilder::BeamBounds>
    BeamLatticeVoxelBuilder::precomputeBeamBounds(std::vector<BeamData> const & beams) const
    {
        std::vector<BeamBounds> bounds;
        bounds.reserve(beams.size());

        for (size_t i = 0; i < beams.size(); ++i)
        {
            BeamData const & beam = beams[i];
            float maxRadius = std::max(beam.startRadius, beam.endRadius);

            BeamBounds bb;
            bb.minX = std::min(beam.startPos.x, beam.endPos.x) - maxRadius;
            bb.maxX = std::max(beam.startPos.x, beam.endPos.x) + maxRadius;
            bb.minY = std::min(beam.startPos.y, beam.endPos.y) - maxRadius;
            bb.maxY = std::max(beam.startPos.y, beam.endPos.y) + maxRadius;
            bb.minZ = std::min(beam.startPos.z, beam.endPos.z) - maxRadius;
            bb.maxZ = std::max(beam.startPos.z, beam.endPos.z) + maxRadius;
            bb.beamIndex = i; // Store the index in original vector order

            bounds.push_back(bb);
        }

        return bounds;
    }

    std::vector<BeamLatticeVoxelBuilder::BallBounds>
    BeamLatticeVoxelBuilder::precomputeBallBounds(std::vector<BallData> const & balls) const
    {
        std::vector<BallBounds> bounds;
        bounds.reserve(balls.size());

        for (size_t i = 0; i < balls.size(); ++i)
        {
            BallData const & ball = balls[i];

            BallBounds bb;
            bb.centerX = ball.position.x;
            bb.centerY = ball.position.y;
            bb.centerZ = ball.position.z;
            bb.radius = ball.radius;
            bb.ballIndex = i; // Store the index in original vector order

            bounds.push_back(bb);
        }

        return bounds;
    }

    std::pair<int, int> BeamLatticeVoxelBuilder::findClosestPrimitiveOptimized(
      openvdb::Vec3f const & point,
      std::vector<BeamBounds> const & beamBounds,
      std::vector<BallBounds> const & ballBounds,
      std::vector<BeamData> const & beams,
      std::vector<BallData> const & balls,
      float maxDist) const
    {
        float bestDist = maxDist;
        int bestIndex = -1;
        int bestType = -1; // 0=beam, 1=ball

        // Phase 1 Optimization: Check beams using pre-computed data but identical logic
        // Process in the same order as original to ensure identical tie-breaking
        for (size_t i = 0; i < beamBounds.size(); ++i)
        {
            BeamBounds const & bb = beamBounds[i];

            // Conservative bounding box rejection test - make it very conservative
            // Only reject if the point is definitively outside the expanded bounds
            const float conservativeMargin = bestDist + 1.0f; // Very conservative margin
            if (point.x() < bb.minX - conservativeMargin ||
                point.x() > bb.maxX + conservativeMargin ||
                point.y() < bb.minY - conservativeMargin ||
                point.y() > bb.maxY + conservativeMargin ||
                point.z() < bb.minZ - conservativeMargin ||
                point.z() > bb.maxZ + conservativeMargin)
            {
                continue; // Skip only obvious rejections
            }

            float d = calculateBeamDistance(point, beams[bb.beamIndex]);
            if (d < bestDist)
            {
                bestDist = d;
                bestIndex = static_cast<int>(bb.beamIndex);
                bestType = 0;
            }
        }

        // Phase 1 Optimization: Check balls using pre-computed data but identical logic
        // Process in the same order as original to ensure identical tie-breaking
        for (size_t i = 0; i < ballBounds.size(); ++i)
        {
            BallBounds const & bb = ballBounds[i];

            // Conservative distance check - make it very conservative
            float dx = std::abs(point.x() - bb.centerX);
            float dy = std::abs(point.y() - bb.centerY);
            float dz = std::abs(point.z() - bb.centerZ);

            // Only skip if obviously too far with large conservative margin
            const float conservativeMargin = bestDist + 1.0f;
            if (dx + dy + dz > bb.radius + conservativeMargin)
            {
                continue; // Skip only obvious rejections
            }

            float d = calculateBallDistance(point, balls[bb.ballIndex]);
            if (d < bestDist)
            {
                bestDist = d;
                bestIndex = static_cast<int>(bb.ballIndex);
                bestType = 1;
            }
        }

        return {bestIndex, bestType};
    }
}
