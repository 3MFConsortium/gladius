/**
 * @file BeamLatticeVoxelAccelerationReference.h
 * @brief Reference implementation of voxel-based acceleration for beam lattices
 * @details This is a copy of the original BeamLatticeVoxelAcceleration implementation
 *          that serves as a baseline for performance optimization and correctness verification
 */

#pragma once

#include "BeamLatticeResource.h"
#include "io/vdb.h"
#include <openvdb/Grid.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/GridTransformer.h>

namespace gladius
{
    /// @brief Configuration for beam lattice voxel acceleration (reference implementation)
    struct BeamLatticeVoxelSettingsReference
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

    /// @brief Reference implementation of BeamLatticeVoxelBuilder for testing and comparison
    class BeamLatticeVoxelBuilderReference
    {
      public:
        /// @brief Build voxel acceleration grid from beam lattice data (reference implementation)
        /// @param beams Vector of beam primitives
        /// @param balls Vector of ball primitives
        /// @param settings Voxelization configuration
        /// @return Pair of (primitive index grid, optional type grid)
        std::pair<openvdb::Int32Grid::Ptr, openvdb::Int32Grid::Ptr>
        buildVoxelGrids(const std::vector<BeamData> & beams,
                        const std::vector<BallData> & balls,
                        BeamLatticeVoxelSettingsReference const & settings = {});

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

        /// @brief Build bounding box encompassing all primitives
        openvdb::BBoxd calculateBoundingBox(std::vector<BeamData> const & beams,
                                            std::vector<BallData> const & balls) const;
    };
}
