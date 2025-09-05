#pragma once

#include "kernel/types.h"
#include <memory>
#include <vector>

namespace gladius
{
    /// BVH node structure for beam lattice acceleration
    struct BeamBVHNode
    {
        BoundingBox boundingBox;
        int leftChild;      // Index to left child (-1 if leaf)
        int rightChild;     // Index to right child (-1 if leaf)
        int primitiveStart; // First primitive index (for leaves)
        int primitiveCount; // Number of primitives (for leaves)
        int depth;          // Node depth for debugging
        int padding[3];     // Alignment to 16-byte boundary

        BeamBVHNode()
            : leftChild(-1)
            , rightChild(-1)
            , primitiveStart(0)
            , primitiveCount(0)
            , depth(0)
        {
            padding[0] = padding[1] = padding[2] = 0;
        }

        bool isLeaf() const
        {
            return leftChild == -1 && rightChild == -1;
        }
    };

    /// Combined primitive data for BVH construction
    struct BeamPrimitive
    {
        enum Type
        {
            BEAM,
            BALL
        };

        Type type;
        int index; // Index into beams or balls array
        BoundingBox bounds;
        float4 centroid;

        BeamPrimitive(Type t, int idx, const BoundingBox & b)
            : type(t)
            , index(idx)
            , bounds(b)
        {
            centroid = float4{(b.min.x + b.max.x) * 0.5f,
                              (b.min.y + b.max.y) * 0.5f,
                              (b.min.z + b.max.z) * 0.5f,
                              0.0f};
        }
    };

    /// BVH builder for beam lattices using Surface Area Heuristic
    class BeamBVHBuilder
    {
      public:
        struct BuildParams
        {
            int maxDepth;
            int maxPrimitivesPerLeaf;
            float traversalCost;
            float intersectionCost;

            BuildParams()
                : maxDepth(20)
                , maxPrimitivesPerLeaf(4)
                , traversalCost(1.0f)
                , intersectionCost(2.0f)
            {
            }
        };

        struct BuildStats
        {
            int totalNodes;
            int leafNodes;
            int maxDepth;
            float avgDepth;
            float sahCost;

            BuildStats()
                : totalNodes(0)
                , leafNodes(0)
                , maxDepth(0)
                , avgDepth(0.0f)
                , sahCost(0.0f)
            {
            }
        };

        BeamBVHBuilder() = default;
        ~BeamBVHBuilder() = default;

        /// Build BVH from beam and ball data
        std::vector<BeamBVHNode> build(const std::vector<BeamData> & beams,
                                       const std::vector<BallData> & balls,
                                       const BuildParams & params = BuildParams{});

        /// Get statistics from last build
        const BuildStats & getLastBuildStats() const
        {
            return m_lastStats;
        }

      private:
        struct BuildContext
        {
            std::vector<BeamPrimitive> primitives;
            BoundingBox sceneBounds;
            BoundingBox centroidBounds;
        };

        BuildStats m_lastStats;

        /// Initialize build context from input data
        BuildContext createBuildContext(const std::vector<BeamData> & beams,
                                        const std::vector<BallData> & balls);

        /// Recursive BVH construction
        int buildRecursive(BuildContext & context,
                           int start,
                           int end,
                           int depth,
                           std::vector<BeamBVHNode> & nodes,
                           const BuildParams & params);

        /// Find best split using SAH
        int findBestSplit(BuildContext & context,
                          int start,
                          int end,
                          int & splitAxis,
                          float & splitPos,
                          const BuildParams & params);

        /// Evaluate Surface Area Heuristic cost
        float evaluateSAH(const BuildContext & context,
                          int start,
                          int split,
                          int end,
                          int axis,
                          float pos,
                          const BuildParams & params);

        /// Calculate surface area of bounding box
        float surfaceArea(const BoundingBox & box);

        /// Update build statistics
        void updateStats(const std::vector<BeamBVHNode> & nodes);
    };

    /// Utility functions for beam lattice geometry
    namespace BeamUtils
    {
        /// Calculate tight bounding box for a conical beam
        BoundingBox calculateBeamBounds(const BeamData & beam);

        /// Calculate bounding box for a ball
        BoundingBox calculateBallBounds(const BallData & ball);

        /// Merge two bounding boxes
        BoundingBox mergeBounds(const BoundingBox & a, const BoundingBox & b);

        /// Check if a point is inside a bounding box
        bool isPointInside(const float4 & point, const BoundingBox & box);

        /// Calculate distance from point to bounding box
        float distanceToBounds(const float4 & point, const BoundingBox & box);
    }
}
