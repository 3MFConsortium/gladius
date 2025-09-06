#pragma once

#include "BeamBVH.h"
#include "ResourceManager.h"
#include "kernel/types.h"

#include <memory>
#include <vector>

namespace gladius
{
    /// @brief Resource class for managing beam lattice data and BVH structures
    /// @details Extends ResourceBase to handle beam lattice loading, BVH construction,
    /// and GPU data transfer following the established resource management pattern
    class BeamLatticeResource : public ResourceBase
    {
      public:
        /// @brief Construct beam lattice resource from data vectors
        /// @param key Resource key for identification and metadata
        /// @param beams Vector of beam primitive data (moved for efficiency)
        /// @param balls Vector of ball primitive data (moved for efficiency)
        BeamLatticeResource(ResourceKey key,
                            std::vector<BeamData> && beams,
                            std::vector<BallData> && balls);

        /// @brief Get read-only access to beam data
        /// @return Const reference to beam vector
        const std::vector<BeamData> & getBeams() const
        {
            return m_beams;
        }

        /// @brief Get read-only access to ball data
        /// @return Const reference to ball vector
        const std::vector<BallData> & getBalls() const
        {
            return m_balls;
        }

        /// @brief Get read-only access to BVH nodes
        /// @return Const reference to BVH node vector
        const std::vector<BeamBVHNode> & getBVHNodes() const
        {
            return m_bvhNodes;
        }

        /// @brief Get statistics about the BVH construction
        /// @return Build statistics including node counts and depth
        const BeamBVHBuilder::BuildStats & getBuildStats() const
        {
            return m_buildStats;
        }

        /// @brief Get total number of primitives (beams + balls)
        /// @return Total primitive count
        size_t getTotalPrimitiveCount() const
        {
            return m_beams.size() + m_balls.size();
        }

        /// @brief Check if the lattice has any ball primitives
        /// @return True if balls are present
        bool hasBalls() const
        {
            return !m_balls.empty();
        }

        /// @brief Write beam lattice data to primitives collection
        /// @details Implements the ResourceBase::write interface to add BVH hierarchy
        /// and primitive data to the global primitives collection for GPU consumption.
        void write(Primitives & primitives) override;

        /// @brief Calculate tight bounding box for a beam primitive (for testing)
        /// @param beam The beam to calculate bounds for
        /// @return Tight bounding box including caps and radii
        BoundingBox calculateBeamBounds(const BeamData & beam);

        /// @brief Calculate bounding box for a ball primitive (for testing)
        /// @param ball The ball to calculate bounds for
        /// @return Sphere bounding box
        BoundingBox calculateBallBounds(const BallData & ball);

      private:
        /// @brief Implementation of resource loading (BVH construction)
        /// @details Called by ResourceBase::load() when resource needs initialization.
        /// Constructs BVH from beam/ball data and prepares GPU payload.
        void loadImpl() override;

        /// @brief Write BVH nodes to payload as primitive metadata
        void writeBVHNodesToPayload();

        /// @brief Write primitive indices mapping to payload data
        void writePrimitiveIndicesToPayload(const std::vector<BeamPrimitive> & primitiveOrdering);

        /// @brief Write beam primitives to payload data
        void writeBeamPrimitivesToPayload();

        /// @brief Write ball primitives to payload data
        void writeBallPrimitivesToPayload();

        // Beam lattice data
        std::vector<BeamData> m_beams; ///< Beam primitive data
        std::vector<BallData> m_balls; ///< Ball primitive data (optional)

        // BVH hierarchy data
        std::vector<BeamBVHNode> m_bvhNodes;     ///< BVH hierarchy for efficient traversal
        BeamBVHBuilder::BuildStats m_buildStats; ///< Statistics from BVH construction

        // GPU data management
        PrimitiveBuffer m_payloadData; ///< Prepared data for GPU transfer

        /// @brief BVH builder parameters for construction
        BeamBVHBuilder::BuildParams m_bvhParams;
    };

    /// @brief Type alias for shared beam lattice resource
    using SharedBeamLatticeResource = std::shared_ptr<BeamLatticeResource>;
}
