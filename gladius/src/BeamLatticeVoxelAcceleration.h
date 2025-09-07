/**
 * @file BeamLatticeVoxelAcceleration.h
 * @brief Voxel-based acceleration structure for beam lattice distance queries
 * @details Similar to mesh face index grids, creates a 3D grid where each voxel
 *          stores the index of the closest beam/ball primitive
 */

#pragma once

#include "BeamLatticeResource.h"
#include "io/vdb.h"
#include <openvdb/Grid.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/GridTransformer.h>

namespace gladius
{
    /// @brief Configuration for beam lattice voxel acceleration
    struct BeamLatticeVoxelSettings
    {
        /// @brief Voxel size in world units (smaller = more accurate but larger memory)
        float voxelSize = 1.0f;

        /// @brief Maximum distance to consider for primitive assignment
        float maxDistance = 10.0f;

        /// @brief Whether to create separate grids for beams and balls
        bool separateBeamBallGrids = true;

        /// @brief Whether to store primitive type in upper bits of index
        bool encodeTypeInIndex = false;

        /// @brief Enable debug output during grid construction
        bool enableDebugOutput = false;
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

        /// @brief Get statistics from last grid build
        struct BuildStats
        {
            size_t totalVoxels = 0;
            size_t activeVoxels = 0;
            float averageDistance = 0.0f;
            float maxDistance = 0.0f;
            size_t memoryUsageBytes = 0;
            float buildTimeSeconds = 0.0f;
        };

        BuildStats const & getLastBuildStats() const
        {
            return m_lastStats;
        }

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

        /// @brief Calculate distance from point to beam primitive
        float calculateBeamDistance(openvdb::Vec3f const & point, BeamData const & beam) const;

        /// @brief Calculate distance from point to ball primitive
        float calculateBallDistance(openvdb::Vec3f const & point, BallData const & ball) const;

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
