/**
 * @file BeamLatticeVoxelAcceleration.h
 * @brief Voxel-based acceleration structure for beam lattice distance queries
 * @details Similar to mesh face index grids, creates a 3D grid where each voxel
 *          stores the index of the closest beam/ball primitive
 */

#pragma once

// Enable Phase 3 optimizations (temporarily disabled to bypass compile issue)
// #define ENABLE_PHASE3_SIMD

#include "BeamLatticeResource.h"
#include "io/vdb.h"
#include <openvdb/Grid.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/GridTransformer.h>
#ifdef ENABLE_PHASE3_SIMD
#include <immintrin.h> // For SIMD intrinsics
#endif
#include <thread> // For parallelization

namespace gladius
{
    /// @brief Configuration for beam lattice voxel acceleration
    struct BeamLatticeVoxelSettings
    {
        /// @brief Voxel size in world units (smaller = more accurate but larger memory)
        float voxelSize = 0.5f;

        /// @brief Maximum distance to consider for primitive assignment
        float maxDistance = 10.0f;

        /// @brief Whether to create separate grids for beams and balls
        bool separateBeamBallGrids = true;

        /// @brief Whether to store primitive type in upper bits of index
        bool encodeTypeInIndex = false;

        /// @brief Enable debug output during grid construction
        bool enableDebugOutput = false;

        /// @brief Optimization phase to use (1=bounding box, 2=primitive-centric, 3=parallel+SIMD)
        int optimizationPhase = 3;

        /// @brief Spatial hash cell size multiplier (relative to voxel size)
        float spatialHashCellSizeMultiplier = 4.0f;

        /// @brief Number of threads for parallel processing (0 = auto-detect)
        unsigned int numThreads = 0;

        /// @brief Enable SIMD optimizations if supported by CPU
        bool enableSIMD = true;

        /// @brief Batch size for SIMD processing
        unsigned int simdBatchSize = 8;
    };

    /// @brief Creates voxel acceleration grids for beam lattice structures
    class BeamLatticeVoxelBuilder
    {
      public:
        /// @brief Build voxel acceleration grid from beam lattice data
        /// @param beams Vector of beam primitives
        /// @param balls Vector of ball primitives
        /// @param settings Voxelization configuration
        /// @return Pair of (primitive index grid, optional type grid)
        std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
        buildVoxelGrids(const std::vector<BeamData> & beams,
                        const std::vector<BallData> & balls,
                        BeamLatticeVoxelSettings const & settings = {});

        /// @brief Phase 1: Build voxel grids using bounding box optimization
        std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
        buildVoxelGridsPhase1(const std::vector<BeamData> & beams,
                              const std::vector<BallData> & balls,
                              BeamLatticeVoxelSettings const & settings);

        /// @brief Phase 2: Build voxel grids using primitive-centric algorithm
        std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
        buildVoxelGridsPhase2(std::vector<BeamData> const & beams,
                              std::vector<BallData> const & balls,
                              BeamLatticeVoxelSettings const & settings);

        /// @brief Phase 3: Build voxel grids using parallel + SIMD optimization
        std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
        buildVoxelGridsPhase3(std::vector<BeamData> const & beams,
                              std::vector<BallData> const & balls,
                              BeamLatticeVoxelSettings const & settings);

        /// @brief Get statistics from last grid build
        struct BuildStats
        {
            size_t totalVoxels = 0;
            size_t activeVoxels = 0;
            float averageDistance = 0.0f;
            float maxDistance = 0.0f;
            size_t memoryUsageBytes = 0;
            float buildTimeSeconds = 0.0f;

            // Phase 2 optimization statistics
            size_t spatialHashCells = 0;          ///< Number of spatial hash cells created
            size_t primitiveVoxelPairs = 0;       ///< Number of primitive-voxel pairs processed
            float hashBuildTimeSeconds = 0.0f;    ///< Time to build spatial hash
            float voxelProcessTimeSeconds = 0.0f; ///< Time to process voxels
        };

        BuildStats const & getLastBuildStats() const
        {
            return m_lastStats;
        }

#ifdef ENABLE_PHASE3_SIMD
        /// @brief CPU capability detection for SIMD optimizations
        struct CPUCapabilities
        {
            bool hasSSE41 = false;
            bool hasAVX = false;
            bool hasAVX2 = false;
            bool hasAVX512F = false;
        };

        /// @brief Get CPU capabilities for SIMD optimization
        static CPUCapabilities detectCPUCapabilities();
#endif

      private:
        BuildStats m_lastStats;

        /// @brief Cached beam bounding box for quick rejection tests
        struct BeamBounds
        {
            float minX, maxX, minY, maxY, minZ, maxZ;
            size_t beamIndex;
        };

        /// @brief Cached ball bounding box for quick rejection tests
        struct BallBounds
        {
            float centerX, centerY, centerZ, radius;
            size_t ballIndex;
        };

        /// @brief Phase 2: Spatial hash cell for primitive-centric algorithm
        struct SpatialHashCell
        {
            std::vector<size_t> beamIndices; ///< Beam indices in this cell
            std::vector<size_t> ballIndices; ///< Ball indices in this cell
        };

#ifdef ENABLE_PHASE3_SIMD
        /// @brief Phase 3: SIMD-aligned beam data for vectorized operations
        struct alignas(32) SIMDBeamData
        {
            float startX, startY, startZ, startRadius;
            float endX, endY, endZ, endRadius;
            float dirX, dirY, dirZ, length;
            size_t originalIndex;
        };

        /// @brief Phase 3: SIMD-aligned ball data for vectorized operations
        struct alignas(32) SIMDBallData
        {
            float x, y, z, radius;
            size_t originalIndex;
        };
#endif

        /// @brief Phase 2: Spatial hash grid for efficient primitive-to-voxel mapping
        struct SpatialHashGrid
        {
            float cellSize;                     ///< Size of each hash cell
            openvdb::Vec3f minBounds;           ///< Minimum bounds of the grid
            openvdb::Vec3f maxBounds;           ///< Maximum bounds of the grid
            openvdb::Coord gridSize;            ///< Number of cells in each dimension
            std::vector<SpatialHashCell> cells; ///< Flattened 3D array of cells

            /// @brief Convert world position to hash grid coordinates
            openvdb::Coord worldToGrid(openvdb::Vec3f const & pos) const
            {
                return openvdb::Coord(static_cast<int>((pos.x() - minBounds.x()) / cellSize),
                                      static_cast<int>((pos.y() - minBounds.y()) / cellSize),
                                      static_cast<int>((pos.z() - minBounds.z()) / cellSize));
            }

            /// @brief Get linear index for 3D coordinates
            size_t getLinearIndex(openvdb::Coord const & coord) const
            {
                return coord.x() + coord.y() * gridSize.x() +
                       coord.z() * gridSize.x() * gridSize.y();
            }

            /// @brief Check if coordinates are valid
            bool isValidCoord(openvdb::Coord const & coord) const
            {
                return coord.x() >= 0 && coord.x() < gridSize.x() && coord.y() >= 0 &&
                       coord.y() < gridSize.y() && coord.z() >= 0 && coord.z() < gridSize.z();
            }
        };

        /// @brief Calculate distance from point to beam primitive
        float calculateBeamDistance(openvdb::Vec3f const & point, BeamData const & beam) const;

        /// @brief Calculate distance from point to ball primitive
        float calculateBallDistance(openvdb::Vec3f const & point, BallData const & ball) const;

#ifdef ENABLE_PHASE3_SIMD
        /// @brief Phase 3: SIMD-optimized beam distance calculation (processes up to 8 beams)
        void calculateBeamDistancesSIMD(openvdb::Vec3f const & point,
                                        SIMDBeamData const * beams,
                                        size_t numBeams,
                                        float * distances) const;

        /// @brief Phase 3: SIMD-optimized ball distance calculation (processes up to 8 balls)
        void calculateBallDistancesSIMD(openvdb::Vec3f const & point,
                                        SIMDBallData const * balls,
                                        size_t numBalls,
                                        float * distances) const;
#endif

        /// @brief Find closest primitive to a point
        /// @param point World space position
        /// @param beams Vector of beam primitives
        /// @param balls Vector of ball primitives
        /// @param maxDist Maximum search distance
        /// @return Pair of (primitive index, type) where type: 0=beam, 1=ball, -1=none
        std::pair<int, int> findClosestPrimitive(openvdb::Vec3f const & point,
                                                 std::vector<BeamData> const & beams,
                                                 std::vector<BallData> const & balls,
                                                 float maxDist) const;

        /// @brief Find closest primitive to a point using cached bounds
        /// @param point World space position
        /// @param beamBounds Cached beam bounding boxes
        /// @param ballBounds Cached ball bounding boxes
        /// @param beams Vector of beam primitives (for distance calculation)
        /// @param balls Vector of ball primitives (for distance calculation)
        /// @param maxDist Maximum search distance
        /// @return Pair of (primitive index, type) where type: 0=beam, 1=ball, -1=none
        std::pair<int, int>
        findClosestPrimitiveOptimized(openvdb::Vec3f const & point,
                                      std::vector<BeamBounds> const & beamBounds,
                                      std::vector<BallBounds> const & ballBounds,
                                      std::vector<BeamData> const & beams,
                                      std::vector<BallData> const & balls,
                                      float maxDist) const;

        /// @brief Pre-compute bounding boxes for all beams
        std::vector<BeamBounds> precomputeBeamBounds(std::vector<BeamData> const & beams) const;

        /// @brief Pre-compute bounding boxes for all balls
        std::vector<BallBounds> precomputeBallBounds(std::vector<BallData> const & balls) const;

        /// @brief Build bounding box encompassing all primitives
        openvdb::BBoxd calculateBoundingBox(std::vector<BeamData> const & beams,
                                            std::vector<BallData> const & balls) const;

        // Phase 2 optimization methods

        /// @brief Build spatial hash grid for primitive-centric algorithm
        SpatialHashGrid buildSpatialHashGrid(std::vector<BeamData> const & beams,
                                             std::vector<BallData> const & balls,
                                             float voxelSize,
                                             float maxDistance) const;

        /// @brief Process primitive influence on nearby voxels using spatial hash
        void processPrimitiveInfluence(openvdb::Int32Grid::Ptr & primitiveIndexGrid,
                                       openvdb::Int32Grid::Ptr & primitiveTypeGrid,
                                       SpatialHashGrid const & spatialHash,
                                       std::vector<BeamData> const & beams,
                                       std::vector<BallData> const & balls,
                                       BeamLatticeVoxelSettings const & settings,
                                       openvdb::math::Transform::Ptr const & transform);

        // Phase 3 optimization methods
#ifdef ENABLE_PHASE3_SIMD
        /// @brief Convert beam data to SIMD-aligned format
        std::vector<SIMDBeamData> prepareSIMDBeamData(std::vector<BeamData> const & beams) const;

        /// @brief Convert ball data to SIMD-aligned format
        std::vector<SIMDBallData> prepareSIMDBallData(std::vector<BallData> const & balls) const;

        /// @brief Phase 3: Process spatial hash cells in parallel with SIMD
        void processSpatialHashCellsParallel(openvdb::Int32Grid::Ptr & primitiveIndexGrid,
                                             openvdb::Int32Grid::Ptr & primitiveTypeGrid,
                                             SpatialHashGrid const & spatialHash,
                                             std::vector<SIMDBeamData> const & simdBeams,
                                             std::vector<SIMDBallData> const & simdBalls,
                                             BeamLatticeVoxelSettings const & settings,
                                             openvdb::math::Transform::Ptr const & transform);

        /// @brief Phase 3: Process a single spatial hash cell with SIMD optimization
        void processSpatialHashCellSIMD(openvdb::Int32Grid::Ptr & primitiveIndexGrid,
                                        openvdb::Int32Grid::Ptr & primitiveTypeGrid,
                                        SpatialHashCell const & cell,
                                        SpatialHashGrid const & spatialHash,
                                        std::vector<SIMDBeamData> const & simdBeams,
                                        std::vector<SIMDBallData> const & simdBalls,
                                        BeamLatticeVoxelSettings const & settings,
                                        openvdb::math::Transform::Ptr const & transform,
                                        size_t cellIndex) const;
#endif

      private:
        /// @brief Statistics from processing a spatial hash cell
        struct CellProcessingStats
        {
            size_t voxelsProcessed = 0;
            size_t primitivesProcessed = 0;
            std::chrono::microseconds processingTime{0};
            size_t activeVoxels = 0;
            size_t primitiveVoxelPairs = 0;
            float totalDistance = 0.0f;
            float maxDistance = 0.0f;
        };

#ifdef ENABLE_PHASE3_SIMD
        /// @brief Thread-safe version of SIMD cell processing that returns statistics
        CellProcessingStats
        processSpatialHashCellSIMDThreadSafe(openvdb::Int32Grid::Ptr & primitiveIndexGrid,
                                             openvdb::Int32Grid::Ptr & primitiveTypeGrid,
                                             SpatialHashCell const & cell,
                                             SpatialHashGrid const & spatialHash,
                                             std::vector<SIMDBeamData> const & simdBeams,
                                             std::vector<SIMDBallData> const & simdBalls,
                                             BeamLatticeVoxelSettings const & settings,
                                             openvdb::math::Transform::Ptr const & transform,
                                             size_t cellIndex) const;

        // End of Phase 3 SIMD optimization methods
#endif
    };

    /// @brief Extended beam lattice resource with voxel acceleration option
    /// Note: Declared for future use; not wired into the current factory.
    class BeamLatticeResourceWithVoxels : public BeamLatticeResource
    {
      public:
        BeamLatticeResourceWithVoxels(ResourceKey key,
                                      std::vector<BeamData> && beams,
                                      std::vector<BallData> && balls,
                                      bool useVoxelAcceleration = false);

        /// @brief Configure voxel acceleration settings
        void setVoxelSettings(BeamLatticeVoxelSettings const & settings)
        {
            m_voxelSettings = settings;
        }

        /// @brief Enable or disable voxel acceleration
        void setUseVoxelAcceleration(bool enable)
        {
            m_useVoxelAcceleration = enable;
        }

        /// @brief Check if voxel acceleration is enabled
        bool isUsingVoxelAcceleration() const
        {
            return m_useVoxelAcceleration;
        }

      protected:
        void loadImpl() override;

        /// @brief Write voxel grid data to payload
        void writeVoxelGridsToPayload();

      private:
        bool m_useVoxelAcceleration{false};
        BeamLatticeVoxelSettings m_voxelSettings{};
        BeamLatticeVoxelBuilder m_voxelBuilder{};

        openvdb::Int32Grid::Ptr m_primitiveIndexGrid{};
        openvdb::Int32Grid::Ptr m_primitiveTypeGrid{}; // Optional separate type grid
    };
}
