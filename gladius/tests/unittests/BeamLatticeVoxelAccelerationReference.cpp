/**
 * @file BeamLatticeVoxelAccelerationReference.cpp
 * @brief Reference implementation of voxel-based acceleration for beam lattices
 * @details This is an exact copy of the original BeamLatticeVoxelAcceleration.cpp implementation
 */

#include "BeamLatticeVoxelAccelerationReference.h"
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
    BeamLatticeVoxelBuilderReference::buildVoxelGrids(
      std::vector<BeamData> const & beams,
      std::vector<BallData> const & balls,
      BeamLatticeVoxelSettingsReference const & settings)
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

                    // Find closest primitive
                    auto const [primitiveIndex, primitiveType] =
                      findClosestPrimitive(pos, beams, balls, settings.maxDistance);

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

    float BeamLatticeVoxelBuilderReference::calculateBeamDistance(openvdb::Vec3f const & point,
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

    float BeamLatticeVoxelBuilderReference::calculateBallDistance(openvdb::Vec3f const & point,
                                                                  BallData const & ball) const
    {
        float dx = point.x() - ball.positionRadius.x;
        float dy = point.y() - ball.positionRadius.y;
        float dz = point.z() - ball.positionRadius.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz) - ball.positionRadius.w;
        return dist;
    }

    std::pair<int, int>
    BeamLatticeVoxelBuilderReference::findClosestPrimitive(openvdb::Vec3f const & point,
                                                           std::vector<BeamData> const & beams,
                                                           std::vector<BallData> const & balls,
                                                           float maxDist) const
    {
        float bestDist = maxDist;
        int bestIndex = -1;
        int bestType = -1; // 0=beam, 1=ball

        for (size_t i = 0; i < beams.size(); ++i)
        {
            float d = calculateBeamDistance(point, beams[i]);
            if (d < bestDist)
            {
                bestDist = d;
                bestIndex = static_cast<int>(i);
                bestType = 0;
            }
        }

        for (size_t i = 0; i < balls.size(); ++i)
        {
            float d = calculateBallDistance(point, balls[i]);
            if (d < bestDist)
            {
                bestDist = d;
                bestIndex = static_cast<int>(i);
                bestType = 1;
            }
        }

        return {bestIndex, bestType};
    }

    openvdb::BBoxd BeamLatticeVoxelBuilderReference::calculateBoundingBox(
      std::vector<BeamData> const & beams,
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
            extend(s.positionRadius.x, s.positionRadius.y, s.positionRadius.z);
            extend(s.positionRadius.x + s.positionRadius.w,
                   s.positionRadius.y + s.positionRadius.w,
                   s.positionRadius.z + s.positionRadius.w);
            extend(s.positionRadius.x - s.positionRadius.w,
                   s.positionRadius.y - s.positionRadius.w,
                   s.positionRadius.z - s.positionRadius.w);
        }

        return {minP, maxP};
    }
}
