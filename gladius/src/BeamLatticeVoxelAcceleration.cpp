/**
 * @file BeamLatticeVoxelAcceleration.cpp
 * @brief Implementation of voxel-based acceleration for beam lattices
 */

#include "BeamLatticeVoxelAcceleration.h"
#include "Primitives.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <immintrin.h> // For SIMD intrinsics
#include <iostream>
#include <mutex> // For thread-safe operations
#include <openvdb/openvdb.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/Composite.h>
#include <thread> // For parallelization

#ifdef _WIN32
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace gladius
{
    std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
    BeamLatticeVoxelBuilder::buildVoxelGrids(std::vector<BeamData> const & beams,
                                             std::vector<BallData> const & balls,
                                             BeamLatticeVoxelSettings const & settings)
    {
#if defined(ENABLE_PHASE3_SIMD)
        if (settings.optimizationPhase >= 3)
        {
            return buildVoxelGridsPhase3(beams, balls, settings);
        }
#endif
        if (settings.optimizationPhase == 2 || settings.optimizationPhase >= 3)
        {
            return buildVoxelGridsPhase2(beams, balls, settings);
        }
        return buildVoxelGridsPhase1(beams, balls, settings);
    }

#ifdef ENABLE_PHASE3_SIMD
    gladius::BeamLatticeVoxelBuilder::CPUCapabilities
    BeamLatticeVoxelBuilder::detectCPUCapabilities()
    {
        CPUCapabilities caps;
#ifdef _WIN32
        int cpuInfo[4];
        __cpuid(cpuInfo, 1);
        caps.hasSSE41 = (cpuInfo[2] & (1 << 19)) != 0;
        caps.hasAVX = (cpuInfo[2] & (1 << 28)) != 0;
        __cpuid(cpuInfo, 7);
        caps.hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
        caps.hasAVX512F = (cpuInfo[1] & (1 << 16)) != 0;
#else
        unsigned int eax, ebx, ecx, edx;
        __get_cpuid(1, &eax, &ebx, &ecx, &edx);
        caps.hasSSE41 = (ecx & (1 << 19)) != 0;
        caps.hasAVX = (ecx & (1 << 28)) != 0;
        __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
        caps.hasAVX2 = (ebx & (1 << 5)) != 0;
        caps.hasAVX512F = (ebx & (1 << 16)) != 0;
#endif
        return caps;
    }

    std::vector<gladius::BeamLatticeVoxelBuilder::SIMDBeamData>
    gladius::BeamLatticeVoxelBuilder::prepareSIMDBeamData(std::vector<BeamData> const & beams) const
    {
        std::vector<SIMDBeamData> simdBeams;
        simdBeams.reserve(beams.size());
        for (size_t i = 0; i < beams.size(); ++i)
        {
            BeamData const & beam = beams[i];
            SIMDBeamData simdBeam;
            simdBeam.startX = static_cast<float>(beam.startPos.x);
            simdBeam.startY = static_cast<float>(beam.startPos.y);
            simdBeam.startZ = static_cast<float>(beam.startPos.z);
            simdBeam.startRadius = beam.startRadius;
            simdBeam.endX = static_cast<float>(beam.endPos.x);
            simdBeam.endY = static_cast<float>(beam.endPos.y);
            simdBeam.endZ = static_cast<float>(beam.endPos.z);
            simdBeam.endRadius = beam.endRadius;
            float dx = simdBeam.endX - simdBeam.startX;
            float dy = simdBeam.endY - simdBeam.startY;
            float dz = simdBeam.endZ - simdBeam.startZ;
            simdBeam.length = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (simdBeam.length > 1e-6f)
            {
                simdBeam.dirX = dx / simdBeam.length;
                simdBeam.dirY = dy / simdBeam.length;
                simdBeam.dirZ = dz / simdBeam.length;
            }
            else
            {
                simdBeam.dirX = simdBeam.dirY = simdBeam.dirZ = 0.0f;
            }
            simdBeam.originalIndex = i;
            simdBeams.push_back(simdBeam);
        }
        return simdBeams;
    }

    std::vector<gladius::BeamLatticeVoxelBuilder::SIMDBallData>
    gladius::BeamLatticeVoxelBuilder::prepareSIMDBallData(std::vector<BallData> const & balls) const
    {
        std::vector<SIMDBallData> simdBalls;
        simdBalls.reserve(balls.size());
        for (size_t i = 0; i < balls.size(); ++i)
        {
            BallData const & ball = balls[i];
            SIMDBallData simdBall;
            simdBall.x = ball.positionRadius.x;
            simdBall.y = ball.positionRadius.y;
            simdBall.z = ball.positionRadius.z;
            simdBall.radius = ball.positionRadius.w;
            simdBall.originalIndex = i;
            simdBalls.push_back(simdBall);
        }
        return simdBalls;
    }

    void gladius::BeamLatticeVoxelBuilder::calculateBeamDistancesSIMD(openvdb::Vec3f const & point,
                                                                      SIMDBeamData const * beams,
                                                                      size_t numBeams,
                                                                      float * distances) const
    {
        // Use Eigen for vectorized operations - simpler and more reliable than hand-written SIMD
        Eigen::Vector3f pointVec(point.x(), point.y(), point.z());

        for (size_t i = 0; i < numBeams; ++i)
        {
            SIMDBeamData const & beam = beams[i];

            // Use Eigen vectors for efficient computation
            Eigen::Vector3f start(beam.startX, beam.startY, beam.startZ);
            Eigen::Vector3f dir(beam.dirX, beam.dirY, beam.dirZ);

            // Calculate closest point on beam using vectorized operations
            Eigen::Vector3f v = pointVec - start;
            float t = std::max(0.0f, std::min(beam.length, v.dot(dir)));
            Eigen::Vector3f closest = start + dir * t;

            // Distance from point to beam axis
            float distToAxis = (pointVec - closest).norm();

            // Interpolate radius along beam
            float t_norm = (beam.length > 1e-6f) ? (t / beam.length) : 0.0f;
            float radius = beam.startRadius + t_norm * (beam.endRadius - beam.startRadius);

            distances[i] = distToAxis - radius;
        }
    }

    void gladius::BeamLatticeVoxelBuilder::calculateBallDistancesSIMD(openvdb::Vec3f const & point,
                                                                      SIMDBallData const * balls,
                                                                      size_t numBalls,
                                                                      float * distances) const
    {
        static CPUCapabilities caps = detectCPUCapabilities();
        if (caps.hasAVX2 && numBalls >= 8)
        {
            size_t simdCount = numBalls & ~7;
#ifdef __AVX2__
            __m256 pointX = _mm256_set1_ps(point.x());
            __m256 pointY = _mm256_set1_ps(point.y());
            __m256 pointZ = _mm256_set1_ps(point.z());
            for (size_t i = 0; i < simdCount; i += 8)
            {
                __m256 ballX = _mm256_load_ps(&balls[i].x);
                __m256 ballY = _mm256_load_ps(&balls[i].y);
                __m256 ballZ = _mm256_load_ps(&balls[i].z);
                __m256 radius = _mm256_load_ps(&balls[i].radius);
                __m256 dX = _mm256_sub_ps(pointX, ballX);
                __m256 dY = _mm256_sub_ps(pointY, ballY);
                __m256 dZ = _mm256_sub_ps(pointZ, ballZ);
                __m256 distSq =
                  _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dX, dX), _mm256_mul_ps(dY, dY)),
                                _mm256_mul_ps(dZ, dZ));
                __m256 dist = _mm256_sqrt_ps(distSq);
                __m256 result = _mm256_sub_ps(dist, radius);
                _mm256_store_ps(&distances[i], result);
            }
            for (size_t i = simdCount; i < numBalls; ++i)
            {
                SIMDBallData const & ball = balls[i];
                BallData scalarBall;
                scalarBall.positionRadius = {ball.x, ball.y, ball.z, ball.radius};
                distances[i] = calculateBallDistance(point, scalarBall);
            }
#else
            for (size_t i = 0; i < numBalls; ++i)
            {
                SIMDBallData const & ball = balls[i];
                BallData scalarBall;
                scalarBall.positionRadius = {ball.x, ball.y, ball.z, ball.radius};
                distances[i] = calculateBallDistance(point, scalarBall);
            }
#endif
        }
        else
        {
            for (size_t i = 0; i < numBalls; ++i)
            {
                SIMDBallData const & ball = balls[i];
                BallData scalarBall;
                scalarBall.positionRadius = {ball.x, ball.y, ball.z, ball.radius};
                distances[i] = calculateBallDistance(point, scalarBall);
            }
        }
    }

    std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
    gladius::BeamLatticeVoxelBuilder::buildVoxelGridsPhase3(
      std::vector<BeamData> const & beams,
      std::vector<BallData> const & balls,
      BeamLatticeVoxelSettings const & settings)
    {
        auto startTime = std::chrono::high_resolution_clock::now();
        m_lastStats = BuildStats{};
        if (beams.empty() && balls.empty())
        {
            return {nullptr, nullptr};
        }
        // Assume OpenVDB is initialized by the application or test fixture.
        // Avoid repeated initialize() calls here to reduce overhead.
        CPUCapabilities caps = detectCPUCapabilities();
        if (settings.enableDebugOutput)
        {
            std::cout << "BeamLatticeVoxelBuilder: Using Phase 3 parallel+SIMD optimization\n";
            std::cout << "  CPU capabilities: AVX=" << caps.hasAVX << ", AVX2=" << caps.hasAVX2
                      << ", AVX512F=" << caps.hasAVX512F << "\n";
        }
        auto hashBuildStart = std::chrono::high_resolution_clock::now();
        std::vector<SIMDBeamData> simdBeams = prepareSIMDBeamData(beams);
        std::vector<SIMDBallData> simdBalls = prepareSIMDBallData(balls);
        SpatialHashGrid spatialHash =
          buildSpatialHashGrid(beams, balls, settings.voxelSize, settings.maxDistance);
        auto hashBuildEnd = std::chrono::high_resolution_clock::now();
        m_lastStats.hashBuildTimeSeconds =
          std::chrono::duration<float>(hashBuildEnd - hashBuildStart).count();
        m_lastStats.spatialHashCells = spatialHash.cells.size();
        auto transform = openvdb::math::Transform::createLinearTransform(settings.voxelSize);
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
        openvdb::BBoxd bbox = calculateBoundingBox(beams, balls);
        openvdb::Coord minCoord = transform->worldToIndexNodeCentered(bbox.min());
        openvdb::Coord maxCoord = transform->worldToIndexNodeCentered(bbox.max());
        int const margin =
          static_cast<int>(std::ceil(settings.maxDistance / settings.voxelSize)) + 2;
        minCoord.offset(-margin);
        maxCoord.offset(margin);
        openvdb::Coord gridSize = maxCoord - minCoord;
        m_lastStats.totalVoxels = static_cast<size_t>(gridSize.x()) * gridSize.y() * gridSize.z();
        auto voxelProcessStart = std::chrono::high_resolution_clock::now();
        processSpatialHashCellsParallel(primitiveIndexGrid,
                                        primitiveTypeGrid,
                                        spatialHash,
                                        simdBeams,
                                        simdBalls,
                                        settings,
                                        transform);
        auto voxelProcessEnd = std::chrono::high_resolution_clock::now();
        m_lastStats.voxelProcessTimeSeconds =
          std::chrono::duration<float>(voxelProcessEnd - voxelProcessStart).count();
        m_lastStats.memoryUsageBytes = primitiveIndexGrid->memUsage();
        if (primitiveTypeGrid)
        {
            m_lastStats.memoryUsageBytes += primitiveTypeGrid->memUsage();
        }
        auto endTime = std::chrono::high_resolution_clock::now();
        auto durationMs =
          std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        m_lastStats.buildTimeSeconds = durationMs.count() / 1000.0f;
        if (settings.enableDebugOutput)
        {
            std::cout << "  Phase 3 total time: " << m_lastStats.buildTimeSeconds << "s\n";
            std::cout << "  Hash build: " << m_lastStats.hashBuildTimeSeconds
                      << "s, Voxel process: " << m_lastStats.voxelProcessTimeSeconds << "s\n";
            std::cout << "  Active voxels: " << m_lastStats.activeVoxels << "\n";
            std::cout << "  Primitive-voxel pairs: " << m_lastStats.primitiveVoxelPairs << "\n";
        }
        return {primitiveIndexGrid, primitiveTypeGrid};
    }

    void gladius::BeamLatticeVoxelBuilder::processSpatialHashCellsParallel(
      openvdb::Int32Grid::Ptr & primitiveIndexGrid,
      openvdb::Int32Grid::Ptr & primitiveTypeGrid,
      SpatialHashGrid const & spatialHash,
      std::vector<SIMDBeamData> const & simdBeams,
      std::vector<SIMDBallData> const & simdBalls,
      BeamLatticeVoxelSettings const & settings,
      openvdb::math::Transform::Ptr const & transform)
    {
        unsigned int numThreads = settings.numThreads
                                    ? settings.numThreads
                                    : std::max(1u, std::thread::hardware_concurrency());
        std::mutex statsMutex;
        size_t totalActiveVoxels = 0;
        size_t totalPrimitiveVoxelPairs = 0;
        float totalDistance = 0.0f;
        float maxDistance = 0.0f;
        std::vector<std::thread> threads;
        std::atomic<size_t> cellIndex{0};
        std::mutex gridWriteMutex; // Protect concurrent writes to OpenVDB grids
        for (unsigned int t = 0; t < numThreads; ++t)
        {
            threads.emplace_back(
              [&, this]()
              {
                  size_t localActiveVoxels = 0;
                  size_t localPrimitiveVoxelPairs = 0;
                  float localTotalDistance = 0.0f;
                  float localMaxDistance = 0.0f;
                  size_t currentCell;
                  while ((currentCell = cellIndex.fetch_add(1)) < spatialHash.cells.size())
                  {
                      SpatialHashCell const & cell = spatialHash.cells[currentCell];
                      if (cell.beamIndices.empty() && cell.ballIndices.empty())
                      {
                          continue;
                      }
                      auto cellStats = processSpatialHashCellSIMDThreadSafe(primitiveIndexGrid,
                                                                            primitiveTypeGrid,
                                                                            cell,
                                                                            spatialHash,
                                                                            simdBeams,
                                                                            simdBalls,
                                                                            settings,
                                                                            transform,
                                                                            currentCell,
                                                                            gridWriteMutex);
                      localActiveVoxels += cellStats.activeVoxels;
                      localPrimitiveVoxelPairs += cellStats.primitiveVoxelPairs;
                      localTotalDistance += cellStats.totalDistance;
                      localMaxDistance = std::max(localMaxDistance, cellStats.maxDistance);
                  }
                  std::lock_guard<std::mutex> lock(statsMutex);
                  totalActiveVoxels += localActiveVoxels;
                  totalPrimitiveVoxelPairs += localPrimitiveVoxelPairs;
                  totalDistance += localTotalDistance;
                  maxDistance = std::max(maxDistance, localMaxDistance);
              });
        }
        for (auto & thread : threads)
        {
            thread.join();
        }
        m_lastStats.activeVoxels = totalActiveVoxels;
        m_lastStats.primitiveVoxelPairs = totalPrimitiveVoxelPairs;
        m_lastStats.maxDistance = maxDistance;
        if (totalActiveVoxels > 0)
        {
            m_lastStats.averageDistance = totalDistance / static_cast<float>(totalActiveVoxels);
        }
    }

    gladius::BeamLatticeVoxelBuilder::CellProcessingStats
    gladius::BeamLatticeVoxelBuilder::processSpatialHashCellSIMDThreadSafe(
      openvdb::Int32Grid::Ptr & primitiveIndexGrid,
      openvdb::Int32Grid::Ptr & primitiveTypeGrid,
      SpatialHashCell const & cell,
      SpatialHashGrid const & spatialHash,
      std::vector<SIMDBeamData> const & simdBeams,
      std::vector<SIMDBallData> const & simdBalls,
      BeamLatticeVoxelSettings const & settings,
      openvdb::math::Transform::Ptr const & transform,
      size_t cellIndex,
      std::mutex & gridWriteMutex) const
    {
        CellProcessingStats stats;
        int z = static_cast<int>(cellIndex / (spatialHash.gridSize.x() * spatialHash.gridSize.y()));
        int y =
          static_cast<int>((cellIndex % (spatialHash.gridSize.x() * spatialHash.gridSize.y())) /
                           spatialHash.gridSize.x());
        int x = static_cast<int>(cellIndex % spatialHash.gridSize.x());
        openvdb::Vec3f cellMinWorld(spatialHash.minBounds.x() + x * spatialHash.cellSize,
                                    spatialHash.minBounds.y() + y * spatialHash.cellSize,
                                    spatialHash.minBounds.z() + z * spatialHash.cellSize);
        openvdb::Vec3f cellMaxWorld(cellMinWorld.x() + spatialHash.cellSize,
                                    cellMinWorld.y() + spatialHash.cellSize,
                                    cellMinWorld.z() + spatialHash.cellSize);
        openvdb::Coord minVoxel = transform->worldToIndexNodeCentered(
          openvdb::Vec3d(cellMinWorld.x(), cellMinWorld.y(), cellMinWorld.z()));
        openvdb::Coord maxVoxel = transform->worldToIndexNodeCentered(
          openvdb::Vec3d(cellMaxWorld.x(), cellMaxWorld.y(), cellMaxWorld.z()));
        auto indexAccessor = primitiveIndexGrid->getAccessor();
        constexpr size_t MAX_BATCH_SIZE = 16;
        float beamDistances[MAX_BATCH_SIZE];
        float ballDistances[MAX_BATCH_SIZE];
        for (openvdb::Coord voxelCoord(minVoxel.x(), 0, 0); voxelCoord.x() <= maxVoxel.x();
             ++voxelCoord.x())
        {
            for (voxelCoord.y() = minVoxel.y(); voxelCoord.y() <= maxVoxel.y(); ++voxelCoord.y())
            {
                for (voxelCoord.z() = minVoxel.z(); voxelCoord.z() <= maxVoxel.z();
                     ++voxelCoord.z())
                {
                    // Skip if this voxel was already assigned
                    bool voxelAlreadyAssigned;
                    {
                        std::lock_guard<std::mutex> lock(gridWriteMutex);
                        voxelAlreadyAssigned = indexAccessor.isValueOn(voxelCoord);
                    }
                    if (voxelAlreadyAssigned)
                        continue;
                    openvdb::Vec3d worldPos = transform->indexToWorld(voxelCoord);
                    openvdb::Vec3f pos(static_cast<float>(worldPos.x()),
                                       static_cast<float>(worldPos.y()),
                                       static_cast<float>(worldPos.z()));
                    float bestDist = settings.maxDistance;
                    int bestIndex = -1;
                    int bestType = -1;
                    if (!cell.beamIndices.empty())
                    {
                        size_t numBeams = std::min(cell.beamIndices.size(), MAX_BATCH_SIZE);
                        std::vector<SIMDBeamData> beamBatch;
                        beamBatch.reserve(numBeams);
                        for (size_t i = 0; i < numBeams; ++i)
                            beamBatch.push_back(simdBeams[cell.beamIndices[i]]);
                        this->calculateBeamDistancesSIMD(
                          pos, beamBatch.data(), numBeams, beamDistances);
                        for (size_t i = 0; i < numBeams; ++i)
                        {
                            if (beamDistances[i] < bestDist)
                            {
                                bestDist = beamDistances[i];
                                bestIndex = static_cast<int>(cell.beamIndices[i]);
                                bestType = 0;
                            }
                            stats.primitiveVoxelPairs++;
                        }
                        for (size_t i = numBeams; i < cell.beamIndices.size(); ++i)
                        {
                            BeamData beam = {{simdBeams[cell.beamIndices[i]].startX,
                                              simdBeams[cell.beamIndices[i]].startY,
                                              simdBeams[cell.beamIndices[i]].startZ},
                                             {simdBeams[cell.beamIndices[i]].endX,
                                              simdBeams[cell.beamIndices[i]].endY,
                                              simdBeams[cell.beamIndices[i]].endZ},
                                             simdBeams[cell.beamIndices[i]].startRadius,
                                             simdBeams[cell.beamIndices[i]].endRadius};
                            float d = calculateBeamDistance(pos, beam);
                            if (d < bestDist)
                            {
                                bestDist = d;
                                bestIndex = static_cast<int>(cell.beamIndices[i]);
                                bestType = 0;
                            }
                            stats.primitiveVoxelPairs++;
                        }
                    }
                    if (!cell.ballIndices.empty())
                    {
                        size_t numBalls = std::min(cell.ballIndices.size(), MAX_BATCH_SIZE);
                        std::vector<SIMDBallData> ballBatch;
                        ballBatch.reserve(numBalls);
                        for (size_t i = 0; i < numBalls; ++i)
                            ballBatch.push_back(simdBalls[cell.ballIndices[i]]);
                        calculateBallDistancesSIMD(pos, ballBatch.data(), numBalls, ballDistances);
                        for (size_t i = 0; i < numBalls; ++i)
                        {
                            if (ballDistances[i] < bestDist)
                            {
                                bestDist = ballDistances[i];
                                bestIndex = static_cast<int>(cell.ballIndices[i]);
                                bestType = 1;
                            }
                            stats.primitiveVoxelPairs++;
                        }
                        for (size_t i = numBalls; i < cell.ballIndices.size(); ++i)
                        {
                            BallData ball = {{simdBalls[cell.ballIndices[i]].x,
                                              simdBalls[cell.ballIndices[i]].y,
                                              simdBalls[cell.ballIndices[i]].z,
                                              simdBalls[cell.ballIndices[i]].radius}};
                            float d = calculateBallDistance(pos, ball);
                            if (d < bestDist)
                            {
                                bestDist = d;
                                bestIndex = static_cast<int>(cell.ballIndices[i]);
                                bestType = 1;
                            }
                            stats.primitiveVoxelPairs++;
                        }
                    }
                    if (bestIndex >= 0)
                    {
                        {
                            std::lock_guard<std::mutex> lock(gridWriteMutex);
                            if (settings.encodeTypeInIndex && !settings.separateBeamBallGrids)
                            {
                                int encodedIndex = bestIndex;
                                if (bestType == 1)
                                    encodedIndex |= (1 << 31);
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
                        }
                        stats.activeVoxels++;
                        stats.totalDistance += std::abs(bestDist);
                        stats.maxDistance = std::max(stats.maxDistance, std::abs(bestDist));
                    }
                }
            }
        }
        return stats;
    }
#endif // defined(ENABLE_PHASE3_SIMD)
} // namespace gladius

namespace gladius
{
    // ---- Utility distance functions ----
    float BeamLatticeVoxelBuilder::calculateBeamDistance(openvdb::Vec3f const & point,
                                                         BeamData const & beam) const
    {
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

        // Linear radius interpolation along the beam
        float radius = beam.startRadius + (beam.endRadius - beam.startRadius) * (t / length_val);
        return coreDist - radius;
    }

    float BeamLatticeVoxelBuilder::calculateBallDistance(openvdb::Vec3f const & point,
                                                         BallData const & ball) const
    {
        float dx = point.x() - ball.positionRadius.x;
        float dy = point.y() - ball.positionRadius.y;
        float dz = point.z() - ball.positionRadius.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz) - ball.positionRadius.w;
        return dist;
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

    // ---- Phase 1: Simple voxelization over full bbox ----
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

        // Assume OpenVDB is initialized by the application or test fixture to avoid repeated
        // initialization overhead here.

        auto transform = openvdb::math::Transform::createLinearTransform(settings.voxelSize);
        openvdb::BBoxd bbox = calculateBoundingBox(beams, balls);

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

        auto indexAccessor = primitiveIndexGrid->getAccessor();

        openvdb::Coord minCoord = transform->worldToIndexNodeCentered(bbox.min());
        openvdb::Coord maxCoord = transform->worldToIndexNodeCentered(bbox.max());
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

        float totalDistance = 0.0f;

        // Precompute beam parameters (dir and length) to avoid per-voxel normalization cost
        struct PreBeam
        {
            openvdb::Vec3f start;
            openvdb::Vec3f dir; // normalized axis (zero if degenerate)
            float length;       // axis length
            float r0;
            float r1;
        };
        std::vector<PreBeam> preBeams;
        preBeams.reserve(beams.size());
        for (auto const & b : beams)
        {
            openvdb::Vec3f s(static_cast<float>(b.startPos.x),
                             static_cast<float>(b.startPos.y),
                             static_cast<float>(b.startPos.z));
            openvdb::Vec3f e(static_cast<float>(b.endPos.x),
                             static_cast<float>(b.endPos.y),
                             static_cast<float>(b.endPos.z));
            openvdb::Vec3f axis = e - s;
            float len = axis.length();
            openvdb::Vec3f d = (len > 1e-6f) ? (axis / len) : openvdb::Vec3f(0.0f);
            preBeams.push_back(PreBeam{s, d, len, b.startRadius, b.endRadius});
        }

        auto distanceToPreBeam = [](openvdb::Vec3f const & p, PreBeam const & pb) -> float
        {
            if (pb.length < 1e-6f)
            {
                float core = (p - pb.start).length();
                float r = std::max(pb.r0, pb.r1);
                return core - r;
            }
            openvdb::Vec3f v = p - pb.start;
            float t = v.dot(pb.dir);
            t = std::max(0.0f, std::min(pb.length, t));
            openvdb::Vec3f closest = pb.start + pb.dir * t;
            float coreDist = (p - closest).length();
            float radius = pb.r0 + (pb.r1 - pb.r0) * (t / pb.length);
            return coreDist - radius;
        };

        // Precompute world-space origin for minCoord and use linear arithmetic for voxel positions
        const double vx = static_cast<double>(settings.voxelSize);
        const openvdb::Vec3d worldMin = transform->indexToWorld(minCoord);

        for (openvdb::Coord coord(minCoord.x(), 0, 0); coord.x() <= maxCoord.x(); ++coord.x())
        {
            const double wx = worldMin.x() + static_cast<double>(coord.x() - minCoord.x()) * vx;
            for (coord.y() = minCoord.y(); coord.y() <= maxCoord.y(); ++coord.y())
            {
                const double wy = worldMin.y() + static_cast<double>(coord.y() - minCoord.y()) * vx;
                for (coord.z() = minCoord.z(); coord.z() <= maxCoord.z(); ++coord.z())
                {
                    const double wz =
                      worldMin.z() + static_cast<double>(coord.z() - minCoord.z()) * vx;
                    openvdb::Vec3f pos(
                      static_cast<float>(wx), static_cast<float>(wy), static_cast<float>(wz));

                    // Find closest primitive (naive)
                    float bestDist = settings.maxDistance;
                    int bestIndex = -1;
                    int bestType = -1; // 0=beam, 1=ball

                    for (size_t i = 0; i < beams.size(); ++i)
                    {
                        float d = distanceToPreBeam(pos, preBeams[i]);
                        if (d < bestDist)
                        {
                            bestDist = d;
                            bestIndex = static_cast<int>(i);
                            bestType = 0;
                        }
                    }
                    for (size_t i = 0; i < balls.size(); ++i)
                    {
                        float d = calculateBallDistance(pos, balls[i]);
                        if (d < bestDist)
                        {
                            bestDist = d;
                            bestIndex = static_cast<int>(i);
                            bestType = 1;
                        }
                    }

                    if (bestIndex >= 0)
                    {
                        if (settings.encodeTypeInIndex && !settings.separateBeamBallGrids)
                        {
                            int encodedIndex = bestIndex;
                            if (bestType == 1)
                                encodedIndex |= (1 << 31);
                            indexAccessor.setValue(coord, encodedIndex);
                        }
                        else
                        {
                            indexAccessor.setValue(coord, bestIndex);
                            if (primitiveTypeGrid)
                            {
                                primitiveTypeGrid->tree().setValueOn(coord, bestType);
                            }
                        }

                        m_lastStats.activeVoxels++;
                        // Use the already computed bestDist to avoid recomputation overhead.
                        totalDistance += std::abs(bestDist);
                        m_lastStats.maxDistance =
                          std::max(m_lastStats.maxDistance, std::abs(bestDist));
                    }
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

        return {primitiveIndexGrid, primitiveTypeGrid};
    }

    // ---- Phase 2: Spatial hash assisted voxelization (scalar) ----
    BeamLatticeVoxelBuilder::SpatialHashGrid
    BeamLatticeVoxelBuilder::buildSpatialHashGrid(std::vector<BeamData> const & beams,
                                                  std::vector<BallData> const & balls,
                                                  float voxelSize,
                                                  float maxDistance) const
    {
        SpatialHashGrid grid;
        float cellSize =
          voxelSize * 4.0f; // default multiplier; fine-tuned via settings at call site
        grid.cellSize = cellSize;

        // Compute expanded bounds
        openvdb::BBoxd bbox = calculateBoundingBox(beams, balls);
        openvdb::Vec3f minB(static_cast<float>(bbox.min().x() - maxDistance),
                            static_cast<float>(bbox.min().y() - maxDistance),
                            static_cast<float>(bbox.min().z() - maxDistance));
        openvdb::Vec3f maxB(static_cast<float>(bbox.max().x() + maxDistance),
                            static_cast<float>(bbox.max().y() + maxDistance),
                            static_cast<float>(bbox.max().z() + maxDistance));
        grid.minBounds = minB;
        grid.maxBounds = maxB;

        auto computeGridSize = [&](openvdb::Vec3f const & minP, openvdb::Vec3f const & maxP)
        {
            int gx = std::max(1, static_cast<int>(std::ceil((maxP.x() - minP.x()) / cellSize)));
            int gy = std::max(1, static_cast<int>(std::ceil((maxP.y() - minP.y()) / cellSize)));
            int gz = std::max(1, static_cast<int>(std::ceil((maxP.z() - minP.z()) / cellSize)));
            return openvdb::Coord(gx, gy, gz);
        };
        grid.gridSize = computeGridSize(minB, maxB);
        size_t totalCells =
          static_cast<size_t>(grid.gridSize.x()) * grid.gridSize.y() * grid.gridSize.z();
        grid.cells.clear();
        grid.cells.resize(totalCells);

        auto addBeamToCells = [&](size_t idx, BeamData const & b)
        {
            float rMin = std::min(b.startRadius, b.endRadius);
            float rMax = std::max(b.startRadius, b.endRadius);
            float pad = std::max(rMax, maxDistance);
            float minX = std::min(b.startPos.x, b.endPos.x) - pad;
            float minY = std::min(b.startPos.y, b.endPos.y) - pad;
            float minZ = std::min(b.startPos.z, b.endPos.z) - pad;
            float maxX = std::max(b.startPos.x, b.endPos.x) + pad;
            float maxY = std::max(b.startPos.y, b.endPos.y) + pad;
            float maxZ = std::max(b.startPos.z, b.endPos.z) + pad;
            openvdb::Coord cmin = grid.worldToGrid({minX, minY, minZ});
            openvdb::Coord cmax = grid.worldToGrid({maxX, maxY, maxZ});
            for (int z = std::max(0, cmin.z()); z <= std::min(grid.gridSize.z() - 1, cmax.z()); ++z)
                for (int y = std::max(0, cmin.y()); y <= std::min(grid.gridSize.y() - 1, cmax.y());
                     ++y)
                    for (int x = std::max(0, cmin.x());
                         x <= std::min(grid.gridSize.x() - 1, cmax.x());
                         ++x)
                    {
                        size_t li =
                          x + y * grid.gridSize.x() + z * grid.gridSize.x() * grid.gridSize.y();
                        grid.cells[li].beamIndices.push_back(idx);
                    }
        };
        auto addBallToCells = [&](size_t idx, BallData const & s)
        {
            float pad = std::max(s.positionRadius.w, maxDistance);
            float minX = s.positionRadius.x - pad;
            float minY = s.positionRadius.y - pad;
            float minZ = s.positionRadius.z - pad;
            float maxX = s.positionRadius.x + pad;
            float maxY = s.positionRadius.y + pad;
            float maxZ = s.positionRadius.z + pad;
            openvdb::Coord cmin = grid.worldToGrid({minX, minY, minZ});
            openvdb::Coord cmax = grid.worldToGrid({maxX, maxY, maxZ});
            for (int z = std::max(0, cmin.z()); z <= std::min(grid.gridSize.z() - 1, cmax.z()); ++z)
                for (int y = std::max(0, cmin.y()); y <= std::min(grid.gridSize.y() - 1, cmax.y());
                     ++y)
                    for (int x = std::max(0, cmin.x());
                         x <= std::min(grid.gridSize.x() - 1, cmax.x());
                         ++x)
                    {
                        size_t li =
                          x + y * grid.gridSize.x() + z * grid.gridSize.x() * grid.gridSize.y();
                        grid.cells[li].ballIndices.push_back(idx);
                    }
        };

        for (size_t i = 0; i < beams.size(); ++i)
            addBeamToCells(i, beams[i]);
        for (size_t i = 0; i < balls.size(); ++i)
            addBallToCells(i, balls[i]);

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
        size_t totalActive = 0;
        float totalDistance = 0.0f;
        float maxDistance = 0.0f;

        // Iterate each cell and process its voxel region
        for (size_t ci = 0; ci < spatialHash.cells.size(); ++ci)
        {
            SpatialHashCell const & cell = spatialHash.cells[ci];
            if (cell.beamIndices.empty() && cell.ballIndices.empty())
                continue;

            int z = static_cast<int>(ci / (spatialHash.gridSize.x() * spatialHash.gridSize.y()));
            int y = static_cast<int>((ci % (spatialHash.gridSize.x() * spatialHash.gridSize.y())) /
                                     spatialHash.gridSize.x());
            int x = static_cast<int>(ci % spatialHash.gridSize.x());

            openvdb::Vec3f cellMinWorld(spatialHash.minBounds.x() + x * spatialHash.cellSize,
                                        spatialHash.minBounds.y() + y * spatialHash.cellSize,
                                        spatialHash.minBounds.z() + z * spatialHash.cellSize);
            openvdb::Vec3f cellMaxWorld(cellMinWorld.x() + spatialHash.cellSize,
                                        cellMinWorld.y() + spatialHash.cellSize,
                                        cellMinWorld.z() + spatialHash.cellSize);

            openvdb::Coord minVoxel = transform->worldToIndexNodeCentered(
              openvdb::Vec3d(cellMinWorld.x(), cellMinWorld.y(), cellMinWorld.z()));
            openvdb::Coord maxVoxel = transform->worldToIndexNodeCentered(
              openvdb::Vec3d(cellMaxWorld.x(), cellMaxWorld.y(), cellMaxWorld.z()));

            for (openvdb::Coord voxelCoord(minVoxel.x(), 0, 0); voxelCoord.x() <= maxVoxel.x();
                 ++voxelCoord.x())
            {
                for (voxelCoord.y() = minVoxel.y(); voxelCoord.y() <= maxVoxel.y();
                     ++voxelCoord.y())
                {
                    for (voxelCoord.z() = minVoxel.z(); voxelCoord.z() <= maxVoxel.z();
                         ++voxelCoord.z())
                    {
                        if (indexAccessor.isValueOn(voxelCoord))
                            continue; // already assigned

                        openvdb::Vec3d worldPos = transform->indexToWorld(voxelCoord);
                        openvdb::Vec3f pos(static_cast<float>(worldPos.x()),
                                           static_cast<float>(worldPos.y()),
                                           static_cast<float>(worldPos.z()));

                        float bestDist = settings.maxDistance;
                        int bestIndex = -1;
                        int bestType = -1;

                        for (size_t bi : cell.beamIndices)
                        {
                            float d = calculateBeamDistance(pos, beams[bi]);
                            if (d < bestDist)
                            {
                                bestDist = d;
                                bestIndex = static_cast<int>(bi);
                                bestType = 0;
                            }
                        }
                        for (size_t si : cell.ballIndices)
                        {
                            float d = calculateBallDistance(pos, balls[si]);
                            if (d < bestDist)
                            {
                                bestDist = d;
                                bestIndex = static_cast<int>(si);
                                bestType = 1;
                            }
                        }

                        if (bestIndex >= 0)
                        {
                            if (settings.encodeTypeInIndex && !settings.separateBeamBallGrids)
                            {
                                int encodedIndex = bestIndex;
                                if (bestType == 1)
                                    encodedIndex |= (1 << 31);
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

                            totalActive++;
                            totalDistance += std::abs(bestDist);
                            maxDistance = std::max(maxDistance, std::abs(bestDist));
                        }
                    }
                }
            }
        }

        m_lastStats.activeVoxels = totalActive;
        if (totalActive > 0)
            m_lastStats.averageDistance = totalDistance / static_cast<float>(totalActive);
        m_lastStats.maxDistance = maxDistance;
    }

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

        openvdb::initialize();

        // Build spatial hash
        auto hashStart = std::chrono::high_resolution_clock::now();
        SpatialHashGrid spatialHash =
          buildSpatialHashGrid(beams, balls, settings.voxelSize, settings.maxDistance);
        auto hashEnd = std::chrono::high_resolution_clock::now();
        m_lastStats.hashBuildTimeSeconds =
          std::chrono::duration<float>(hashEnd - hashStart).count();
        m_lastStats.spatialHashCells = spatialHash.cells.size();

        // Create grids and transform
        auto transform = openvdb::math::Transform::createLinearTransform(settings.voxelSize);
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

        // Estimate total voxels for stats using overall bbox
        openvdb::BBoxd bbox = calculateBoundingBox(beams, balls);
        openvdb::Coord minCoord = transform->worldToIndexNodeCentered(bbox.min());
        openvdb::Coord maxCoord = transform->worldToIndexNodeCentered(bbox.max());
        int const margin =
          static_cast<int>(std::ceil(settings.maxDistance / settings.voxelSize)) + 2;
        minCoord.offset(-margin);
        maxCoord.offset(margin);
        openvdb::Coord gridSize = maxCoord - minCoord;
        m_lastStats.totalVoxels = static_cast<size_t>(gridSize.x()) * gridSize.y() * gridSize.z();

        auto voxStart = std::chrono::high_resolution_clock::now();
        processPrimitiveInfluence(
          primitiveIndexGrid, primitiveTypeGrid, spatialHash, beams, balls, settings, transform);
        auto voxEnd = std::chrono::high_resolution_clock::now();
        m_lastStats.voxelProcessTimeSeconds =
          std::chrono::duration<float>(voxEnd - voxStart).count();

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
        m_lastStats.buildTimeSeconds = std::chrono::duration<float>(endTime - startTime).count();

        return {primitiveIndexGrid, primitiveTypeGrid};
    }
}
