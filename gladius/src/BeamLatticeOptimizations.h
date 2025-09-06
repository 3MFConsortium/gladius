/**
 * @file BeamLatticeOptimizations.h
 * @brief Performance optimization strategies for large beam lattice rendering
 */

#pragma once

#include "BeamBVH.h"
#include <vector>

namespace gladius
{
    /// @brief Level-of-detail settings for beam lattice rendering
    struct BeamLatticeRenderSettings
    {
        /// @brief Use simplified distance approximations when primitives are far
        bool enableLOD = true;

        /// @brief Distance threshold for switching to simplified evaluation
        float lodDistanceThreshold = 10.0f;

        /// @brief Minimum screen-space size for rendering primitives (in pixels)
        float minScreenSize = 1.0f;

        /// @brief Use spatial coherence optimizations for neighboring pixels
        bool enableSpatialCoherence = true;

        /// @brief Tile size for spatial coherence (4x4 or 8x8 pixels)
        int tileSize = 4;

        /// @brief Early termination distance for sufficient accuracy
        float earlyTerminationThreshold = 0.001f;

        /// @brief Maximum primitives to evaluate per pixel before fallback
        int maxPrimitivesPerPixel = 64;
    };

    /// @brief Simplified primitive representation for LOD
    struct SimplifiedPrimitive
    {
        float4 center;     ///< Center position
        float radius;      ///< Bounding sphere radius
        float length;      ///< Approximate length (for beams)
        int originalIndex; ///< Index to full primitive data
        int type;          ///< 0 = beam, 1 = ball
    };

    /// @brief LOD-aware BVH builder with performance optimizations
    class OptimizedBeamBVHBuilder : public BeamBVHBuilder
    {
      public:
        /// @brief Build BVH with multiple LOD levels
        std::vector<BeamBVHNode> buildWithLOD(const std::vector<BeamData> & beams,
                                              const std::vector<BallData> & balls,
                                              const BeamLatticeRenderSettings & settings,
                                              const BuildParams & params = BuildParams{});

        /// @brief Get simplified primitives for LOD rendering
        const std::vector<SimplifiedPrimitive> & getSimplifiedPrimitives() const
        {
            return m_simplifiedPrimitives;
        }

        /// @brief Get spatial coherence acceleration structure
        struct SpatialCoherenceData
        {
            std::vector<int> tileToNodeMapping; ///< Tile -> BVH node mapping
            std::vector<int> recentlyUsedNodes; ///< Cache of recently accessed nodes
            float3 lastQueryPoint;              ///< Last query point for coherence
        };

      private:
        std::vector<SimplifiedPrimitive> m_simplifiedPrimitives;

        /// @brief Create simplified primitives for LOD
        void createSimplifiedPrimitives(const std::vector<BeamData> & beams,
                                        const std::vector<BallData> & balls,
                                        const BeamLatticeRenderSettings & settings);

        /// @brief Group small primitives into meta-primitives
        void groupSmallPrimitives(float minSize);
    };

    /// @brief Adaptive BVH parameters based on lattice size and complexity
    class AdaptiveBVHParams
    {
      public:
        /// @brief Calculate optimal BVH parameters for given lattice
        static BuildParams
        calculateOptimalParams(size_t numBeams, size_t numBalls, const BoundingBox & sceneBounds);

        /// @brief Get memory-optimized parameters for large lattices
        static BuildParams getMemoryOptimizedParams();

        /// @brief Get speed-optimized parameters for real-time rendering
        static BuildParams getSpeedOptimizedParams();
    };
}
