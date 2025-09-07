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
        // Choose optimization phase based on settings
        if (settings.optimizationPhase >= 2)
        {
            return buildVoxelGridsPhase2(beams, balls, settings);
        }
        else
        {
            return buildVoxelGridsPhase1(beams, balls, settings);
        }
    }

    std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
    BeamLatticeVoxelBuilder::buildVoxelGridsPhase1(std::vector<BeamData> const & beams,
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

        // Calculate total voxels for statistics
        openvdb::Coord gridSize = maxCoord - minCoord;
        m_lastStats.totalVoxels = static_cast<size_t>(gridSize.x()) * gridSize.y() * gridSize.z();

        if (settings.enableDebugOutput)
        {
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
            // Account for beam thickness by adding maximum radius to both endpoints
            float maxRadius = std::max(b.startRadius, b.endRadius);
            extend(b.startPos.x + maxRadius, b.startPos.y + maxRadius, b.startPos.z + maxRadius);
            extend(b.startPos.x - maxRadius, b.startPos.y - maxRadius, b.startPos.z - maxRadius);
            extend(b.endPos.x + maxRadius, b.endPos.y + maxRadius, b.endPos.z + maxRadius);
            extend(b.endPos.x - maxRadius, b.endPos.y - maxRadius, b.endPos.z - maxRadius);
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

    // Phase 2 Optimization: Primitive-centric algorithm with spatial hash grid

    std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
    BeamLatticeVoxelBuilder::buildVoxelGridsPhase2(std::vector<BeamData> const & beams,
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

        // Calculate grid bounds and total voxels for statistics
        openvdb::Coord minCoord = transform->worldToIndexCellCentered(bbox.min());
        openvdb::Coord maxCoord = transform->worldToIndexCellCentered(bbox.max());
        openvdb::Coord gridSize = maxCoord - minCoord;
        m_lastStats.totalVoxels = static_cast<size_t>(gridSize.x()) * gridSize.y() * gridSize.z();

        if (settings.enableDebugOutput)
        {
            std::cout << "BeamLatticeVoxelBuilder: Using Phase 2 primitive-centric optimization\n";
            std::cout << "  Processing " << beams.size() << " beams and " << balls.size()
                      << " balls\n";
            std::cout << "  Total Voxels: " << m_lastStats.totalVoxels << "\n";
        }

        // Phase 2: Build spatial hash grid for efficient primitive-to-voxel mapping
        auto hashStartTime = std::chrono::high_resolution_clock::now();
        SpatialHashGrid spatialHash =
          buildSpatialHashGrid(beams, balls, settings.voxelSize, settings.maxDistance);
        auto hashEndTime = std::chrono::high_resolution_clock::now();
        auto hashDuration =
          std::chrono::duration_cast<std::chrono::milliseconds>(hashEndTime - hashStartTime);
        m_lastStats.hashBuildTimeSeconds = hashDuration.count() / 1000.0f;
        m_lastStats.spatialHashCells = spatialHash.cells.size();

        if (settings.enableDebugOutput)
        {
            std::cout << "  Built spatial hash with " << spatialHash.cells.size() << " cells in "
                      << m_lastStats.hashBuildTimeSeconds << "s\n";
        }

        // Create grids
        auto primitiveIndexGrid = openvdb::Int32Grid::create(0);
        primitiveIndexGrid->setTransform(transform);
        primitiveIndexGrid->setName("beam_lattice_primitive_indices");

        openvdb::Int32Grid::Ptr primitiveTypeGrid = nullptr;
        if (settings.separateBeamBallGrids)
        {
            primitiveTypeGrid = openvdb::Int32Grid::create(-1);
            primitiveTypeGrid->setTransform(transform);
            primitiveTypeGrid->setName("beam_lattice_primitive_types");
        }

        // Phase 2: Process primitive influence on nearby voxels
        auto voxelStartTime = std::chrono::high_resolution_clock::now();
        processPrimitiveInfluence(
          primitiveIndexGrid, primitiveTypeGrid, spatialHash, beams, balls, settings, transform);
        auto voxelEndTime = std::chrono::high_resolution_clock::now();
        auto voxelDuration =
          std::chrono::duration_cast<std::chrono::milliseconds>(voxelEndTime - voxelStartTime);
        m_lastStats.voxelProcessTimeSeconds = voxelDuration.count() / 1000.0f;

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

        if (settings.enableDebugOutput)
        {
            std::cout << "  Phase 2 total time: " << m_lastStats.buildTimeSeconds << "s\n";
            std::cout << "  Hash build: " << m_lastStats.hashBuildTimeSeconds << "s, ";
            std::cout << "Voxel process: " << m_lastStats.voxelProcessTimeSeconds << "s\n";
            std::cout << "  Active voxels: " << m_lastStats.activeVoxels << "\n";
        }

        return {primitiveIndexGrid, primitiveTypeGrid};
    }

    BeamLatticeVoxelBuilder::SpatialHashGrid
    BeamLatticeVoxelBuilder::buildSpatialHashGrid(std::vector<BeamData> const & beams,
                                                  std::vector<BallData> const & balls,
                                                  float voxelSize,
                                                  float maxDistance) const
    {
        SpatialHashGrid grid;

        // Calculate bounding box
        openvdb::BBoxd bbox = calculateBoundingBox(beams, balls);

        // Set up spatial hash grid with larger cells for efficiency
        grid.cellSize = voxelSize * 4.0f; // Use larger cells than voxels for efficiency
        grid.minBounds = openvdb::Vec3f(
          bbox.min().x() - maxDistance, bbox.min().y() - maxDistance, bbox.min().z() - maxDistance);
        grid.maxBounds = openvdb::Vec3f(
          bbox.max().x() + maxDistance, bbox.max().y() + maxDistance, bbox.max().z() + maxDistance);

        // Calculate grid dimensions
        openvdb::Vec3f span = grid.maxBounds - grid.minBounds;
        grid.gridSize = openvdb::Coord(static_cast<int>(std::ceil(span.x() / grid.cellSize)) + 1,
                                       static_cast<int>(std::ceil(span.y() / grid.cellSize)) + 1,
                                       static_cast<int>(std::ceil(span.z() / grid.cellSize)) + 1);

        // Allocate cells
        size_t totalCells =
          static_cast<size_t>(grid.gridSize.x()) * grid.gridSize.y() * grid.gridSize.z();
        grid.cells.resize(totalCells);

        // Insert beams into spatial hash
        for (size_t i = 0; i < beams.size(); ++i)
        {
            BeamData const & beam = beams[i];
            float maxRadius = std::max(beam.startRadius, beam.endRadius);

            // Calculate AABB for the beam (including radius)
            openvdb::Vec3f minP(std::min(beam.startPos.x, beam.endPos.x) - maxRadius - maxDistance,
                                std::min(beam.startPos.y, beam.endPos.y) - maxRadius - maxDistance,
                                std::min(beam.startPos.z, beam.endPos.z) - maxRadius - maxDistance);
            openvdb::Vec3f maxP(std::max(beam.startPos.x, beam.endPos.x) + maxRadius + maxDistance,
                                std::max(beam.startPos.y, beam.endPos.y) + maxRadius + maxDistance,
                                std::max(beam.startPos.z, beam.endPos.z) + maxRadius + maxDistance);

            // Find hash cells that this beam might influence
            openvdb::Coord minCell = grid.worldToGrid(minP);
            openvdb::Coord maxCell = grid.worldToGrid(maxP);

            // Clamp to valid range
            minCell.x() = std::max(0, minCell.x());
            minCell.y() = std::max(0, minCell.y());
            minCell.z() = std::max(0, minCell.z());
            maxCell.x() = std::min(grid.gridSize.x() - 1, maxCell.x());
            maxCell.y() = std::min(grid.gridSize.y() - 1, maxCell.y());
            maxCell.z() = std::min(grid.gridSize.z() - 1, maxCell.z());

            // Add beam to all cells it might influence
            for (int x = minCell.x(); x <= maxCell.x(); ++x)
            {
                for (int y = minCell.y(); y <= maxCell.y(); ++y)
                {
                    for (int z = minCell.z(); z <= maxCell.z(); ++z)
                    {
                        openvdb::Coord coord(x, y, z);
                        size_t cellIndex = grid.getLinearIndex(coord);
                        grid.cells[cellIndex].beamIndices.push_back(i);
                    }
                }
            }
        }

        // Insert balls into spatial hash
        for (size_t i = 0; i < balls.size(); ++i)
        {
            BallData const & ball = balls[i];

            // Calculate AABB for the ball
            openvdb::Vec3f minP(ball.position.x - ball.radius - maxDistance,
                                ball.position.y - ball.radius - maxDistance,
                                ball.position.z - ball.radius - maxDistance);
            openvdb::Vec3f maxP(ball.position.x + ball.radius + maxDistance,
                                ball.position.y + ball.radius + maxDistance,
                                ball.position.z + ball.radius + maxDistance);

            // Find hash cells that this ball might influence
            openvdb::Coord minCell = grid.worldToGrid(minP);
            openvdb::Coord maxCell = grid.worldToGrid(maxP);

            // Clamp to valid range
            minCell.x() = std::max(0, minCell.x());
            minCell.y() = std::max(0, minCell.y());
            minCell.z() = std::max(0, minCell.z());
            maxCell.x() = std::min(grid.gridSize.x() - 1, maxCell.x());
            maxCell.y() = std::min(grid.gridSize.y() - 1, maxCell.y());
            maxCell.z() = std::min(grid.gridSize.z() - 1, maxCell.z());

            // Add ball to all cells it might influence
            for (int x = minCell.x(); x <= maxCell.x(); ++x)
            {
                for (int y = minCell.y(); y <= maxCell.y(); ++y)
                {
                    for (int z = minCell.z(); z <= maxCell.z(); ++z)
                    {
                        openvdb::Coord coord(x, y, z);
                        size_t cellIndex = grid.getLinearIndex(coord);
                        grid.cells[cellIndex].ballIndices.push_back(i);
                    }
                }
            }
        }

        return grid;
    }

    void BeamLatticeVoxelBuilder::processPrimitiveInfluence(
      openvdb::Int32Grid::Ptr & primitiveIndexGrid,
      openvdb::Int32Grid::Ptr & primitiveTypeGrid,
      SpatialHashGrid const & spatialHash,
      std::vector<BeamData> const & beams,
      std::vector<BallData> const & balls,
      BeamLatticeVoxelSettings const & settings,
      openvdb::math::Transform::Ptr const & transform)
    {
        auto indexAccessor = primitiveIndexGrid->getAccessor();
        float totalDistance = 0.0f;

        // Process each spatial hash cell
        for (size_t cellIndex = 0; cellIndex < spatialHash.cells.size(); ++cellIndex)
        {
            SpatialHashCell const & cell = spatialHash.cells[cellIndex];

            // Skip empty cells
            if (cell.beamIndices.empty() && cell.ballIndices.empty())
            {
                continue;
            }

            // Convert linear cell index back to 3D coordinates
            int z =
              static_cast<int>(cellIndex / (spatialHash.gridSize.x() * spatialHash.gridSize.y()));
            int y =
              static_cast<int>((cellIndex % (spatialHash.gridSize.x() * spatialHash.gridSize.y())) /
                               spatialHash.gridSize.x());
            int x = static_cast<int>(cellIndex % spatialHash.gridSize.x());

            // Calculate world bounds of this hash cell
            openvdb::Vec3f cellMinWorld(spatialHash.minBounds.x() + x * spatialHash.cellSize,
                                        spatialHash.minBounds.y() + y * spatialHash.cellSize,
                                        spatialHash.minBounds.z() + z * spatialHash.cellSize);
            openvdb::Vec3f cellMaxWorld(cellMinWorld.x() + spatialHash.cellSize,
                                        cellMinWorld.y() + spatialHash.cellSize,
                                        cellMinWorld.z() + spatialHash.cellSize);

            // Convert to voxel coordinate range
            openvdb::Coord minVoxel = transform->worldToIndexNodeCentered(
              openvdb::Vec3d(cellMinWorld.x(), cellMinWorld.y(), cellMinWorld.z()));
            openvdb::Coord maxVoxel = transform->worldToIndexNodeCentered(
              openvdb::Vec3d(cellMaxWorld.x(), cellMaxWorld.y(), cellMaxWorld.z()));

            // Process all voxels in this spatial hash cell
            for (openvdb::Coord voxelCoord(minVoxel.x(), 0, 0); voxelCoord.x() <= maxVoxel.x();
                 ++voxelCoord.x())
            {
                for (voxelCoord.y() = minVoxel.y(); voxelCoord.y() <= maxVoxel.y();
                     ++voxelCoord.y())
                {
                    for (voxelCoord.z() = minVoxel.z(); voxelCoord.z() <= maxVoxel.z();
                         ++voxelCoord.z())
                    {
                        // Skip if this voxel already has a primitive assigned
                        if (indexAccessor.getValue(voxelCoord) != 0)
                        {
                            continue;
                        }

                        // Convert voxel coordinate to world space
                        openvdb::Vec3d worldPos = transform->indexToWorld(voxelCoord);
                        openvdb::Vec3f pos(static_cast<float>(worldPos.x()),
                                           static_cast<float>(worldPos.y()),
                                           static_cast<float>(worldPos.z()));

                        float bestDist = settings.maxDistance;
                        int bestIndex = -1;
                        int bestType = -1;

                        // Check all beams in this hash cell
                        for (size_t beamIdx : cell.beamIndices)
                        {
                            float d = calculateBeamDistance(pos, beams[beamIdx]);
                            if (d < bestDist)
                            {
                                bestDist = d;
                                bestIndex = static_cast<int>(beamIdx);
                                bestType = 0;
                            }
                            m_lastStats.primitiveVoxelPairs++;
                        }

                        // Check all balls in this hash cell
                        for (size_t ballIdx : cell.ballIndices)
                        {
                            float d = calculateBallDistance(pos, balls[ballIdx]);
                            if (d < bestDist)
                            {
                                bestDist = d;
                                bestIndex = static_cast<int>(ballIdx);
                                bestType = 1;
                            }
                            m_lastStats.primitiveVoxelPairs++;
                        }

                        // Assign primitive to voxel if found
                        if (bestIndex >= 0)
                        {
                            if (settings.encodeTypeInIndex && !settings.separateBeamBallGrids)
                            {
                                int encodedIndex = bestIndex;
                                if (bestType == 1) // ball
                                {
                                    encodedIndex |= (1 << 31);
                                }
                                indexAccessor.setValue(voxelCoord, encodedIndex);
                            }
                            else
                            {
                                indexAccessor.setValue(voxelCoord, bestIndex);
                                if (primitiveTypeGrid)
                                {
                                    primitiveTypeGrid->tree().setValueOn(voxelCoord, bestType);
                                }
                            }

                            m_lastStats.activeVoxels++;
                            totalDistance += std::abs(bestDist);
                            m_lastStats.maxDistance =
                              std::max(m_lastStats.maxDistance, std::abs(bestDist));
                        }
                    }
                }
            }
        }

        if (m_lastStats.activeVoxels > 0)
        {
            m_lastStats.averageDistance =
              totalDistance / static_cast<float>(m_lastStats.activeVoxels);
        }
    }
}
