#include "BeamBVH.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>

namespace gladius
{
    namespace BeamUtils
    {
        BoundingBox calculateBeamBounds(const BeamData & beam)
        {
            // Calculate tight bounding box for conical beam
            float maxRadius = std::max(beam.startRadius, beam.endRadius);

            BoundingBox bounds;
            bounds.min = float4{std::min(beam.startPos.x, beam.endPos.x) - maxRadius,
                                std::min(beam.startPos.y, beam.endPos.y) - maxRadius,
                                std::min(beam.startPos.z, beam.endPos.z) - maxRadius,
                                0.0f};
            bounds.max = float4{std::max(beam.startPos.x, beam.endPos.x) + maxRadius,
                                std::max(beam.startPos.y, beam.endPos.y) + maxRadius,
                                std::max(beam.startPos.z, beam.endPos.z) + maxRadius,
                                0.0f};

            return bounds;
        }

        BoundingBox calculateBallBounds(const BallData & ball)
        {
            BoundingBox bounds;
            bounds.min = float4{ball.position.x - ball.radius,
                                ball.position.y - ball.radius,
                                ball.position.z - ball.radius,
                                0.0f};
            bounds.max = float4{ball.position.x + ball.radius,
                                ball.position.y + ball.radius,
                                ball.position.z + ball.radius,
                                0.0f};

            return bounds;
        }

        BoundingBox mergeBounds(const BoundingBox & a, const BoundingBox & b)
        {
            BoundingBox result;
            result.min = float4{std::min(a.min.x, b.min.x),
                                std::min(a.min.y, b.min.y),
                                std::min(a.min.z, b.min.z),
                                0.0f};
            result.max = float4{std::max(a.max.x, b.max.x),
                                std::max(a.max.y, b.max.y),
                                std::max(a.max.z, b.max.z),
                                0.0f};
            return result;
        }

        bool isPointInside(const float4 & point, const BoundingBox & box)
        {
            return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y &&
                   point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z;
        }

        float distanceToBounds(const float4 & point, const BoundingBox & box)
        {
            float dx = std::max(0.0f, std::max(box.min.x - point.x, point.x - box.max.x));
            float dy = std::max(0.0f, std::max(box.min.y - point.y, point.y - box.max.y));
            float dz = std::max(0.0f, std::max(box.min.z - point.z, point.z - box.max.z));
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }
    }

    BeamBVHBuilder::BuildContext
    BeamBVHBuilder::createBuildContext(const std::vector<BeamData> & beams,
                                       const std::vector<BallData> & balls)
    {
        BuildContext context;
        context.primitives.reserve(beams.size() + balls.size());

        // Initialize scene bounds to empty
        context.sceneBounds = BoundingBox{};
        context.centroidBounds = BoundingBox{};

        bool firstPrimitive = true;

        // Add beam primitives
        for (size_t i = 0; i < beams.size(); ++i)
        {
            BoundingBox bounds = BeamUtils::calculateBeamBounds(beams[i]);
            context.primitives.emplace_back(BeamPrimitive::BEAM, static_cast<int>(i), bounds);

            if (firstPrimitive)
            {
                context.sceneBounds = bounds;
                context.centroidBounds.min = context.primitives.back().centroid;
                context.centroidBounds.max = context.primitives.back().centroid;
                firstPrimitive = false;
            }
            else
            {
                context.sceneBounds = BeamUtils::mergeBounds(context.sceneBounds, bounds);

                // Update centroid bounds
                const auto & centroid = context.primitives.back().centroid;
                context.centroidBounds.min =
                  float4{std::min(context.centroidBounds.min.x, centroid.x),
                         std::min(context.centroidBounds.min.y, centroid.y),
                         std::min(context.centroidBounds.min.z, centroid.z),
                         0.0f};
                context.centroidBounds.max =
                  float4{std::max(context.centroidBounds.max.x, centroid.x),
                         std::max(context.centroidBounds.max.y, centroid.y),
                         std::max(context.centroidBounds.max.z, centroid.z),
                         0.0f};
            }
        }

        // Add ball primitives
        for (size_t i = 0; i < balls.size(); ++i)
        {
            BoundingBox bounds = BeamUtils::calculateBallBounds(balls[i]);
            context.primitives.emplace_back(BeamPrimitive::BALL, static_cast<int>(i), bounds);

            if (firstPrimitive)
            {
                context.sceneBounds = bounds;
                context.centroidBounds.min = context.primitives.back().centroid;
                context.centroidBounds.max = context.primitives.back().centroid;
                firstPrimitive = false;
            }
            else
            {
                context.sceneBounds = BeamUtils::mergeBounds(context.sceneBounds, bounds);

                // Update centroid bounds
                const auto & centroid = context.primitives.back().centroid;
                context.centroidBounds.min =
                  float4{std::min(context.centroidBounds.min.x, centroid.x),
                         std::min(context.centroidBounds.min.y, centroid.y),
                         std::min(context.centroidBounds.min.z, centroid.z),
                         0.0f};
                context.centroidBounds.max =
                  float4{std::max(context.centroidBounds.max.x, centroid.x),
                         std::max(context.centroidBounds.max.y, centroid.y),
                         std::max(context.centroidBounds.max.z, centroid.z),
                         0.0f};
            }
        }

        return context;
    }

    float BeamBVHBuilder::surfaceArea(const BoundingBox & box)
    {
        float dx = box.max.x - box.min.x;
        float dy = box.max.y - box.min.y;
        float dz = box.max.z - box.min.z;

        // Clamp to avoid negative values due to floating point precision
        dx = std::max(0.0f, dx);
        dy = std::max(0.0f, dy);
        dz = std::max(0.0f, dz);

        return 2.0f * (dx * dy + dy * dz + dz * dx);
    }

    float BeamBVHBuilder::evaluateSAH(const BuildContext & context,
                                      int start,
                                      int split,
                                      int end,
                                      int axis,
                                      float pos,
                                      const BuildParams & params)
    {
        // Calculate bounding boxes for left and right partitions
        BoundingBox leftBounds{}, rightBounds{};
        int leftCount = 0, rightCount = 0;
        bool leftFirst = true, rightFirst = true;

        for (int i = start; i < end; ++i)
        {
            const auto & primitive = context.primitives[i];
            float centroidValue = (axis == 0)   ? primitive.centroid.x
                                  : (axis == 1) ? primitive.centroid.y
                                                : primitive.centroid.z;

            if (centroidValue < pos)
            {
                // Left partition
                leftCount++;
                if (leftFirst)
                {
                    leftBounds = primitive.bounds;
                    leftFirst = false;
                }
                else
                {
                    leftBounds = BeamUtils::mergeBounds(leftBounds, primitive.bounds);
                }
            }
            else
            {
                // Right partition
                rightCount++;
                if (rightFirst)
                {
                    rightBounds = primitive.bounds;
                    rightFirst = false;
                }
                else
                {
                    rightBounds = BeamUtils::mergeBounds(rightBounds, primitive.bounds);
                }
            }
        }

        // Avoid empty partitions
        if (leftCount == 0 || rightCount == 0)
        {
            return std::numeric_limits<float>::infinity();
        }

        // Calculate SAH cost
        float leftSA = surfaceArea(leftBounds);
        float rightSA = surfaceArea(rightBounds);
        float totalSA = surfaceArea(context.sceneBounds);

        if (totalSA <= 0.0f)
        {
            return std::numeric_limits<float>::infinity();
        }

        float cost = params.traversalCost + params.intersectionCost *
                                              (leftCount * leftSA + rightCount * rightSA) / totalSA;

        return cost;
    }

    int BeamBVHBuilder::findBestSplit(BuildContext & context,
                                      int start,
                                      int end,
                                      int & splitAxis,
                                      float & splitPos,
                                      const BuildParams & params)
    {
        float bestCost = std::numeric_limits<float>::infinity();
        int bestSplit = start;
        splitAxis = 0;
        splitPos = 0.0f;

        // Try splitting on each axis
        for (int axis = 0; axis < 3; ++axis)
        {
            // Create a copy for sorting to find the best split position
            std::vector<BeamPrimitive> sortedPrimitives(context.primitives.begin() + start,
                                                        context.primitives.begin() + end);

            // Sort primitives by centroid along this axis
            std::sort(sortedPrimitives.begin(),
                      sortedPrimitives.end(),
                      [axis](const BeamPrimitive & a, const BeamPrimitive & b)
                      {
                          float aVal = (axis == 0)   ? a.centroid.x
                                       : (axis == 1) ? a.centroid.y
                                                     : a.centroid.z;
                          float bVal = (axis == 0)   ? b.centroid.x
                                       : (axis == 1) ? b.centroid.y
                                                     : b.centroid.z;
                          return aVal < bVal;
                      });

            // Try different split positions
            const int numSamples = std::min(32, static_cast<int>(sortedPrimitives.size()) - 1);
            for (int i = 1; i <= numSamples; ++i)
            {
                int sampleIndex =
                  (i * static_cast<int>(sortedPrimitives.size())) / (numSamples + 1);
                const auto & primitive = sortedPrimitives[sampleIndex];
                float pos = (axis == 0)   ? primitive.centroid.x
                            : (axis == 1) ? primitive.centroid.y
                                          : primitive.centroid.z;

                float cost =
                  evaluateSAH(context, start, start + sampleIndex, end, axis, pos, params);

                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestSplit = start + sampleIndex;
                    splitAxis = axis;
                    splitPos = pos;
                }
            }
        }

        // Sort by best split axis for the final partition
        std::sort(context.primitives.begin() + start,
                  context.primitives.begin() + end,
                  [splitAxis](BeamPrimitive & a, BeamPrimitive & b)
                  {
                      float aVal = (splitAxis == 0)   ? a.centroid.x
                                   : (splitAxis == 1) ? a.centroid.y
                                                      : a.centroid.z;
                      float bVal = (splitAxis == 0)   ? b.centroid.x
                                   : (splitAxis == 1) ? b.centroid.y
                                                      : b.centroid.z;
                      return aVal < bVal;
                  });

        // Find actual split point based on position
        for (int i = start + 1; i < end; ++i)
        {
            const auto & primitive = context.primitives[i];
            float centroidValue = (splitAxis == 0)   ? primitive.centroid.x
                                  : (splitAxis == 1) ? primitive.centroid.y
                                                     : primitive.centroid.z;
            if (centroidValue >= splitPos)
            {
                return i;
            }
        }

        return (start + end) / 2; // Fallback to middle split
    }

    int BeamBVHBuilder::buildRecursive(BuildContext & context,
                                       int start,
                                       int end,
                                       int depth,
                                       std::vector<BeamBVHNode> & nodes,
                                       const BuildParams & params)
    {
        assert(start < end);

        // Create new node
        int nodeIndex = static_cast<int>(nodes.size());
        nodes.emplace_back();
        BeamBVHNode & node = nodes[nodeIndex];
        node.depth = depth;

        // Calculate bounding box for this range
        bool first = true;
        for (int i = start; i < end; ++i)
        {
            if (first)
            {
                node.boundingBox = context.primitives[i].bounds;
                first = false;
            }
            else
            {
                node.boundingBox =
                  BeamUtils::mergeBounds(node.boundingBox, context.primitives[i].bounds);
            }
        }

        int primitiveCount = end - start;

        // Check termination criteria
        if (primitiveCount <= params.maxPrimitivesPerLeaf || depth >= params.maxDepth)
        {
            // Create leaf node
            node.primitiveStart = start;
            node.primitiveCount = primitiveCount;
            return nodeIndex;
        }

        // Find best split
        int splitAxis;
        float splitPos;
        int split = findBestSplit(context, start, end, splitAxis, splitPos, params);

        // Ensure we have a valid split
        if (split <= start || split >= end)
        {
            split = (start + end) / 2;
        }

        // Build children
        node.leftChild = buildRecursive(context, start, split, depth + 1, nodes, params);
        node.rightChild = buildRecursive(context, split, end, depth + 1, nodes, params);

        return nodeIndex;
    }

    void BeamBVHBuilder::updateStats(const std::vector<BeamBVHNode> & nodes)
    {
        m_lastStats = BuildStats{};
        m_lastStats.totalNodes = static_cast<int>(nodes.size());

        if (nodes.empty())
            return;

        int totalDepth = 0;
        for (const auto & node : nodes)
        {
            if (node.isLeaf())
            {
                m_lastStats.leafNodes++;
            }
            m_lastStats.maxDepth = std::max(m_lastStats.maxDepth, node.depth);
            totalDepth += node.depth;
        }

        m_lastStats.avgDepth = static_cast<float>(totalDepth) / nodes.size();
    }

    std::vector<BeamBVHNode> BeamBVHBuilder::build(const std::vector<BeamData> & beams,
                                                   const std::vector<BallData> & balls,
                                                   const BuildParams & params)
    {
        std::vector<BeamBVHNode> nodes;

        if (beams.empty() && balls.empty())
        {
            m_lastStats = BuildStats{};
            return nodes;
        }

        // Create build context
        BuildContext context = createBuildContext(beams, balls);

        // Build BVH recursively
        buildRecursive(context, 0, static_cast<int>(context.primitives.size()), 0, nodes, params);

        // Update statistics
        updateStats(nodes);

        return nodes;
    }
}
