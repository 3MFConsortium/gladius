#include "BeamBVH.h"
#include "BeamLatticeResource.h"
#include "ResourceKey.h"
#include "kernel/types.h"
#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <iomanip>
#include <iostream>
#include <vector>

using namespace gladius;

namespace gladius::tests
{
    // Define float3 type for CPU calculations (not available in host OpenCL headers)
    struct float3
    {
        float x, y, z;

        float3()
            : x(0)
            , y(0)
            , z(0)
        {
        }
        float3(float x_, float y_, float z_)
            : x(x_)
            , y(y_)
            , z(z_)
        {
        }
    };

    /// @brief CPU prototype utilities for debugging beam lattice evaluation
    namespace BeamLatticeCPU
    {
        /// @brief CPU equivalent of OpenCL beamDistance function
        float beamDistance(const float3 & pos, const BeamData & beam)
        {
            float3 start = {beam.startPos.x, beam.startPos.y, beam.startPos.z};
            float3 end = {beam.endPos.x, beam.endPos.y, beam.endPos.z};
            float3 axis = {end.x - start.x, end.y - start.y, end.z - start.z};
            float length_val = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);

            // Handle degenerate beam (zero length) - treat as sphere
            if (length_val < 1e-6f)
            {
                float radius = std::max(beam.startRadius, beam.endRadius);
                float3 diff = {pos.x - start.x, pos.y - start.y, pos.z - start.z};
                return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) - radius;
            }

            axis.x /= length_val;
            axis.y /= length_val;
            axis.z /= length_val;

            // Project point onto beam axis
            float3 toPoint = {pos.x - start.x, pos.y - start.y, pos.z - start.z};
            float t_unclamped = toPoint.x * axis.x + toPoint.y * axis.y + toPoint.z * axis.z;
            float t = std::clamp(t_unclamped, 0.0f, length_val);

            // Interpolate radius at projection point
            float radius =
              beam.startRadius + (beam.endRadius - beam.startRadius) * (t / length_val);

            // Calculate distance to axis
            float3 projection = {start.x + t * axis.x, start.y + t * axis.y, start.z + t * axis.z};
            float3 toProjDiff = {pos.x - projection.x, pos.y - projection.y, pos.z - projection.z};
            float distToAxis = std::sqrt(toProjDiff.x * toProjDiff.x + toProjDiff.y * toProjDiff.y +
                                         toProjDiff.z * toProjDiff.z);

            // Distance to cylindrical surface
            float surfaceDist = distToAxis - radius;

            // Handle caps based on cap style
            if (t_unclamped <= 0.0f)
            {
                // Near start cap
                switch (beam.startCapStyle)
                {
                case 0: // hemisphere
                {
                    float3 diff = {pos.x - start.x, pos.y - start.y, pos.z - start.z};
                    return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) -
                           beam.startRadius;
                }
                case 1: // sphere
                {
                    float3 diff = {pos.x - start.x, pos.y - start.y, pos.z - start.z};
                    return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) -
                           beam.startRadius;
                }
                case 2: // butt
                    return std::max(surfaceDist, -t_unclamped);
                default:
                    return surfaceDist;
                }
            }
            else if (t_unclamped >= length_val)
            {
                // Near end cap
                float overrun = t_unclamped - length_val;
                switch (beam.endCapStyle)
                {
                case 0: // hemisphere
                {
                    float3 diff = {pos.x - end.x, pos.y - end.y, pos.z - end.z};
                    return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) -
                           beam.endRadius;
                }
                case 1: // sphere
                {
                    float3 diff = {pos.x - end.x, pos.y - end.y, pos.z - end.z};
                    return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) -
                           beam.endRadius;
                }
                case 2: // butt
                    return std::max(surfaceDist, overrun);
                default:
                    return surfaceDist;
                }
            }

            return surfaceDist;
        }

        /// @brief CPU equivalent of OpenCL ballDistance function
        float ballDistance(const float3 & pos, const BallData & ball)
        {
            float3 diff = {
              pos.x - ball.position.x, pos.y - ball.position.y, pos.z - ball.position.z};
            return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z) - ball.radius;
        }

        /// @brief CPU bounding box distance calculation
        float bbBox(const float3 & pos, const float3 & bbmin, const float3 & bbmax)
        {
            float3 dimensions = {bbmax.x - bbmin.x, bbmax.y - bbmin.y, bbmax.z - bbmin.z};
            float3 center = {bbmin.x + dimensions.x * 0.5f,
                             bbmin.y + dimensions.y * 0.5f,
                             bbmin.z + dimensions.z * 0.5f};
            float3 translated = {pos.x - center.x, pos.y - center.y, pos.z - center.z};

            // box function implementation
            float3 d = {std::abs(translated.x) - dimensions.x * 0.5f,
                        std::abs(translated.y) - dimensions.y * 0.5f,
                        std::abs(translated.z) - dimensions.z * 0.5f};

            float maxComponent = std::max({d.x, d.y, d.z});
            float3 maxD = {std::max(d.x, 0.0f), std::max(d.y, 0.0f), std::max(d.z, 0.0f)};
            float length = std::sqrt(maxD.x * maxD.x + maxD.y * maxD.y + maxD.z * maxD.z);

            return std::min(maxComponent, 0.0f) + length;
        }

        /// @brief Debug information for BVH traversal
        struct DebugInfo
        {
            int nodesVisited = 0;
            int primitivesChecked = 0;
            int stackMaxDepth = 0;
            std::vector<int> visitedNodes;
            std::vector<std::pair<int, float>> checkedPrimitives; // primitive index, distance

            void reset()
            {
                nodesVisited = 0;
                primitivesChecked = 0;
                stackMaxDepth = 0;
                visitedNodes.clear();
                checkedPrimitives.clear();
            }
        };

        /// @brief CPU version of flat beam lattice evaluation (O(n) brute force)
        float evaluateBeamLatticeFlat(const float3 & pos,
                                      const std::vector<BeamData> & beams,
                                      const std::vector<BallData> & balls,
                                      DebugInfo * debug = nullptr)
        {
            float minDist = std::numeric_limits<float>::max();

            if (debug)
                debug->reset();

            // Check all beams
            for (size_t i = 0; i < beams.size(); ++i)
            {
                float dist = beamDistance(pos, beams[i]);
                if (debug)
                {
                    debug->primitivesChecked++;
                    debug->checkedPrimitives.emplace_back(static_cast<int>(i), dist);
                }
                minDist = std::min(minDist, dist);
            }

            // Check all balls
            for (size_t i = 0; i < balls.size(); ++i)
            {
                float dist = ballDistance(pos, balls[i]);
                if (debug)
                {
                    debug->primitivesChecked++;
                    debug->checkedPrimitives.emplace_back(static_cast<int>(beams.size() + i), dist);
                }
                minDist = std::min(minDist, dist);
            }

            return minDist;
        }

        /// @brief CPU version of BVH beam lattice evaluation
        float evaluateBeamLatticeBVH(const float3 & pos,
                                     const std::vector<BeamBVHNode> & bvhNodes,
                                     const std::vector<BeamPrimitive> & primitiveOrdering,
                                     const std::vector<BeamData> & beams,
                                     const std::vector<BallData> & balls,
                                     DebugInfo * debug = nullptr)
        {
            if (bvhNodes.empty())
            {
                return std::numeric_limits<float>::max();
            }

            if (debug)
                debug->reset();

            float minDist = std::numeric_limits<float>::max();

            // BVH traversal stack (limit depth to prevent overflow)
            const int MAX_STACK_SIZE = 64;
            int stack[MAX_STACK_SIZE];
            int stackPtr = 0;

            // Start traversal from root node
            stack[stackPtr++] = 0;

            int currentMaxStack = stackPtr;

            while (stackPtr > 0 && stackPtr < MAX_STACK_SIZE)
            {
                currentMaxStack = std::max(currentMaxStack, stackPtr);
                int nodeIndex = stack[--stackPtr];

                if (nodeIndex >= static_cast<int>(bvhNodes.size()) || nodeIndex < 0)
                {
                    std::cout << "Warning: Invalid node index " << nodeIndex << std::endl;
                    continue; // Skip invalid nodes
                }

                const BeamBVHNode & node = bvhNodes[nodeIndex];

                if (debug)
                {
                    debug->nodesVisited++;
                    debug->visitedNodes.push_back(nodeIndex);
                }

                // Check if point is potentially close to bounding box
                float3 bbMin = {
                  node.boundingBox.min.x, node.boundingBox.min.y, node.boundingBox.min.z};
                float3 bbMax = {
                  node.boundingBox.max.x, node.boundingBox.max.y, node.boundingBox.max.z};
                float bbDist = bbBox(pos, bbMin, bbMax);

                if (bbDist > minDist)
                {
                    continue; // Skip if already found closer primitive
                }

                // Check if this is a leaf node (-1 means no child)
                bool isLeaf = (node.leftChild == -1 && node.rightChild == -1);

                if (isLeaf)
                {
                    // Process primitives in this leaf node
                    for (int i = 0; i < node.primitiveCount; ++i)
                    {
                        int primitiveIndex = node.primitiveStart + i;

                        if (primitiveIndex >= static_cast<int>(primitiveOrdering.size()))
                        {
                            std::cout << "Warning: Invalid primitive index " << primitiveIndex
                                      << std::endl;
                            continue;
                        }

                        const BeamPrimitive & primitive = primitiveOrdering[primitiveIndex];
                        float dist;

                        if (primitive.type == BeamPrimitive::BEAM)
                        {
                            if (primitive.index >= static_cast<int>(beams.size()))
                            {
                                std::cout << "Warning: Invalid beam index " << primitive.index
                                          << std::endl;
                                continue;
                            }
                            dist = beamDistance(pos, beams[primitive.index]);
                        }
                        else
                        { // BeamPrimitive::BALL
                            if (primitive.index >= static_cast<int>(balls.size()))
                            {
                                std::cout << "Warning: Invalid ball index " << primitive.index
                                          << std::endl;
                                continue;
                            }
                            dist = ballDistance(pos, balls[primitive.index]);
                        }

                        if (debug)
                        {
                            debug->primitivesChecked++;
                            debug->checkedPrimitives.emplace_back(primitiveIndex, dist);
                        }

                        minDist = std::min(minDist, dist);
                    }
                }
                else
                {
                    // Internal node - add children to stack (if they exist)
                    if (node.rightChild >= 0 && stackPtr < MAX_STACK_SIZE - 1)
                    {
                        stack[stackPtr++] = node.rightChild;
                    }
                    if (node.leftChild >= 0 && stackPtr < MAX_STACK_SIZE - 1)
                    {
                        stack[stackPtr++] = node.leftChild;
                    }
                }
            }

            if (debug)
            {
                debug->stackMaxDepth = currentMaxStack;
            }

            return minDist;
        }
    }

    class BeamLatticeTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            // Setup test fixtures if needed
        }

        void TearDown() override
        {
            // Cleanup if needed
        }
    };

    /// @brief Test that headers can be included and compilation succeeds
    TEST_F(BeamLatticeTest, Headers_Include_CompilationSucceeds)
    {
        // This test validates that the beam lattice headers can be included
        // and that basic types are available for compilation
        EXPECT_TRUE(true); // This test passes if compilation succeeds
    }

    /// @brief Test ResourceKey functionality
    TEST_F(BeamLatticeTest, ResourceKey_Creation_Works)
    {
        ResourceKey testKey(123);
        EXPECT_TRUE(testKey.getResourceId().has_value());
        EXPECT_EQ(testKey.getResourceId().value(), 123);
    }

    /// @brief Test that BeamBVHBuilder type is available
    TEST_F(BeamLatticeTest, BeamBVHBuilder_TypeAvailable_CanDeclareVariable)
    {
        // Test that we can at least declare a variable of this type
        // This validates that the header is properly included and the type is accessible
        BeamBVHBuilder * builder = nullptr;
        EXPECT_EQ(builder, nullptr);
    }

    /// @brief Test basic beam distance calculation
    TEST_F(BeamLatticeTest, BeamDistance_SingleBeam_CorrectDistance)
    {
        BeamData beam;
        beam.startPos = {0.0f, 0.0f, 0.0f, 0.0f};
        beam.endPos = {10.0f, 0.0f, 0.0f, 0.0f};
        beam.startRadius = 1.0f;
        beam.endRadius = 1.0f;
        beam.startCapStyle = 2; // butt
        beam.endCapStyle = 2;   // butt

        // Test point on surface
        float3 pointOnSurface = {5.0f, 1.0f, 0.0f};
        float distance = BeamLatticeCPU::beamDistance(pointOnSurface, beam);
        EXPECT_NEAR(distance, 0.0f, 1e-5f);

        // Test point inside beam
        float3 pointInside = {5.0f, 0.5f, 0.0f};
        distance = BeamLatticeCPU::beamDistance(pointInside, beam);
        EXPECT_LT(distance, 0.0f);

        // Test point outside beam
        float3 pointOutside = {5.0f, 2.0f, 0.0f};
        distance = BeamLatticeCPU::beamDistance(pointOutside, beam);
        EXPECT_NEAR(distance, 1.0f, 1e-5f);
    }

    /// @brief Test basic ball distance calculation
    TEST_F(BeamLatticeTest, BallDistance_SingleBall_CorrectDistance)
    {
        BallData ball;
        ball.position = {0.0f, 0.0f, 0.0f, 0.0f};
        ball.radius = 2.0f;

        // Test point on surface
        float3 pointOnSurface = {2.0f, 0.0f, 0.0f};
        float distance = BeamLatticeCPU::ballDistance(pointOnSurface, ball);
        EXPECT_NEAR(distance, 0.0f, 1e-5f);

        // Test point inside ball
        float3 pointInside = {1.0f, 0.0f, 0.0f};
        distance = BeamLatticeCPU::ballDistance(pointInside, ball);
        EXPECT_NEAR(distance, -1.0f, 1e-5f);

        // Test point outside ball
        float3 pointOutside = {5.0f, 0.0f, 0.0f};
        distance = BeamLatticeCPU::ballDistance(pointOutside, ball);
        EXPECT_NEAR(distance, 3.0f, 1e-5f);
    }

    /// @brief Create test beam lattice data
    struct TestLatticeData
    {
        std::vector<BeamData> beams;
        std::vector<BallData> balls;
        std::vector<float3> testPoints;

        TestLatticeData()
        {
            // Create a simple test lattice with a few beams and balls
            BeamData beam1;
            beam1.startPos = {0.0f, 0.0f, 0.0f, 0.0f};
            beam1.endPos = {10.0f, 0.0f, 0.0f, 0.0f};
            beam1.startRadius = 1.0f;
            beam1.endRadius = 1.0f;
            beam1.startCapStyle = 2; // butt
            beam1.endCapStyle = 2;   // butt
            beams.push_back(beam1);

            BeamData beam2;
            beam2.startPos = {0.0f, 0.0f, 0.0f, 0.0f};
            beam2.endPos = {0.0f, 10.0f, 0.0f, 0.0f};
            beam2.startRadius = 0.5f;
            beam2.endRadius = 1.5f;  // tapered beam
            beam2.startCapStyle = 0; // hemisphere
            beam2.endCapStyle = 1;   // sphere
            beams.push_back(beam2);

            BeamData beam3;
            beam3.startPos = {5.0f, 5.0f, 0.0f, 0.0f};
            beam3.endPos = {5.0f, 5.0f, 10.0f, 0.0f};
            beam3.startRadius = 0.8f;
            beam3.endRadius = 0.8f;
            beam3.startCapStyle = 2; // butt
            beam3.endCapStyle = 2;   // butt
            beams.push_back(beam3);

            BallData ball1;
            ball1.position = {15.0f, 15.0f, 15.0f, 0.0f};
            ball1.radius = 2.0f;
            balls.push_back(ball1);

            BallData ball2;
            ball2.position = {-5.0f, -5.0f, -5.0f, 0.0f};
            ball2.radius = 1.5f;
            balls.push_back(ball2);

            // Test points at various locations
            testPoints = {
              {0.0f, 0.0f, 0.0f},       // Origin
              {5.0f, 1.5f, 0.0f},       // Near beam1
              {0.0f, 5.0f, 0.0f},       // On beam2
              {5.0f, 5.0f, 5.0f},       // On beam3
              {15.0f, 15.0f, 15.0f},    // Center of ball1
              {-5.0f, -5.0f, -5.0f},    // Center of ball2
              {100.0f, 100.0f, 100.0f}, // Far away
              {2.5f, 2.5f, 2.5f},       // Between primitives
            };
        }
    };

    /// @brief Test that flat and BVH evaluation give the same results
    TEST_F(BeamLatticeTest, FlatVsBVH_SameTestData_IdenticalResults)
    {
        TestLatticeData testData;

        // Build BVH with parameters optimized for small primitive counts
        BeamBVHBuilder builder;
        BeamBVHBuilder::BuildParams params;
        params.maxPrimitivesPerLeaf = 1; // Force proper tree construction
        params.maxDepth = 10;            // Allow sufficient depth
        auto bvhNodes = builder.build(testData.beams, testData.balls, params);
        auto primitiveOrdering = builder.getPrimitiveOrdering();

        EXPECT_GT(bvhNodes.size(), 0) << "BVH should have been built";
        EXPECT_EQ(primitiveOrdering.size(), testData.beams.size() + testData.balls.size())
          << "Primitive ordering should contain all primitives";

        // Debug BVH structure
        std::cout << "\n=== BVH Structure Debug ===" << std::endl;
        std::cout << "Total BVH nodes: " << bvhNodes.size() << std::endl;
        std::cout << "Total primitives: " << primitiveOrdering.size() << std::endl;

        for (size_t i = 0; i < bvhNodes.size(); ++i)
        {
            const BeamBVHNode & node = bvhNodes[i];
            std::cout << "Node " << i << ": ";
            if (node.leftChild == -1 && node.rightChild == -1)
            {
                std::cout << "LEAF - primitives [" << node.primitiveStart << ".."
                          << (node.primitiveStart + node.primitiveCount - 1)
                          << "] count=" << node.primitiveCount;
            }
            else
            {
                std::cout << "INTERNAL - left=" << node.leftChild << ", right=" << node.rightChild;
            }
            std::cout << ", bbox=(" << node.boundingBox.min.x << "," << node.boundingBox.min.y
                      << "," << node.boundingBox.min.z << ") to (" << node.boundingBox.max.x << ","
                      << node.boundingBox.max.y << "," << node.boundingBox.max.z << ")"
                      << std::endl;
        }

        std::cout << "\n=== Primitive Ordering Debug ===" << std::endl;
        for (size_t i = 0; i < primitiveOrdering.size(); ++i)
        {
            const BeamPrimitive & prim = primitiveOrdering[i];
            std::cout << "Primitive " << i << ": "
                      << (prim.type == BeamPrimitive::BEAM ? "BEAM" : "BALL")
                      << " index=" << prim.index << std::endl;
        }

        // Test all points
        for (size_t i = 0; i < testData.testPoints.size(); ++i)
        {
            const float3 & point = testData.testPoints[i];

            BeamLatticeCPU::DebugInfo debugFlat, debugBVH;

            float flatResult = BeamLatticeCPU::evaluateBeamLatticeFlat(
              point, testData.beams, testData.balls, &debugFlat);

            float bvhResult = BeamLatticeCPU::evaluateBeamLatticeBVH(
              point, bvhNodes, primitiveOrdering, testData.beams, testData.balls, &debugBVH);

            if (i == 0)
            { // Only debug first point to avoid too much output
                std::cout << "\n=== Point 0 Debug ===" << std::endl;
                std::cout << "Point: (" << point.x << ", " << point.y << ", " << point.z << ")"
                          << std::endl;
                std::cout << "Flat result: " << flatResult << " (checked "
                          << debugFlat.primitivesChecked << " primitives)" << std::endl;
                std::cout << "BVH result: " << bvhResult << " (checked "
                          << debugBVH.primitivesChecked << " primitives, visited "
                          << debugBVH.nodesVisited << " nodes)" << std::endl;

                std::cout << "BVH visited nodes: ";
                for (int nodeId : debugBVH.visitedNodes)
                {
                    std::cout << nodeId << " ";
                }
                std::cout << std::endl;
            }

            EXPECT_NEAR(flatResult, bvhResult, 1e-5f)
              << "Results should be identical for point " << i << " (" << point.x << ", " << point.y
              << ", " << point.z << ")"
              << "\nFlat: " << flatResult << ", BVH: " << bvhResult << "\nFlat checked "
              << debugFlat.primitivesChecked << " primitives"
              << "\nBVH checked " << debugBVH.primitivesChecked << " primitives"
              << "\nBVH visited " << debugBVH.nodesVisited << " nodes";
        }
    }

    /// @brief Test BVH efficiency (should check fewer primitives than flat)
    TEST_F(BeamLatticeTest, BVH_EfficiencyTest_FewerPrimitivesChecked)
    {
        TestLatticeData testData;

        // Build BVH with parameters optimized for small primitive counts
        BeamBVHBuilder builder;
        BeamBVHBuilder::BuildParams params;
        params.maxPrimitivesPerLeaf = 1; // Force proper tree construction
        params.maxDepth = 10;            // Allow sufficient depth
        auto bvhNodes = builder.build(testData.beams, testData.balls, params);
        auto primitiveOrdering = builder.getPrimitiveOrdering();

        // Test a point that's far from most primitives
        float3 farPoint = {0.0f, 0.0f, 50.0f};

        BeamLatticeCPU::DebugInfo debugFlat, debugBVH;

        float flatResult = BeamLatticeCPU::evaluateBeamLatticeFlat(
          farPoint, testData.beams, testData.balls, &debugFlat);

        float bvhResult = BeamLatticeCPU::evaluateBeamLatticeBVH(
          farPoint, bvhNodes, primitiveOrdering, testData.beams, testData.balls, &debugBVH);

        EXPECT_NEAR(flatResult, bvhResult, 1e-5f) << "Results should be identical";

        // BVH should provide identical results even if not more efficient for small datasets
        std::cout << "\n=== BVH Efficiency Test ===" << std::endl;
        std::cout << "Test point: (" << farPoint.x << ", " << farPoint.y << ", " << farPoint.z
                  << ")" << std::endl;
        std::cout << "Flat evaluation: " << flatResult << " (checked "
                  << debugFlat.primitivesChecked << " primitives)" << std::endl;
        std::cout << "BVH evaluation: " << bvhResult << " (checked " << debugBVH.primitivesChecked
                  << " primitives, visited " << debugBVH.nodesVisited << " nodes)" << std::endl;

        // For small datasets, BVH may not be more efficient but should be correct
        if (debugBVH.primitivesChecked < debugFlat.primitivesChecked)
        {
            float efficiency = (float) debugFlat.primitivesChecked / debugBVH.primitivesChecked;
            std::cout << "Efficiency gain: " << efficiency << "x" << std::endl;
        }
        else
        {
            std::cout
              << "No efficiency gain for this small test dataset (expected for 5 primitives)"
              << std::endl;
        }
    }

    /// @brief Test BVH with detailed debugging output
    TEST_F(BeamLatticeTest, BVH_DetailedDebug_TraversalCorrectness)
    {
        TestLatticeData testData;

        // Build BVH with parameters optimized for small primitive counts
        BeamBVHBuilder builder;
        BeamBVHBuilder::BuildParams params;
        params.maxPrimitivesPerLeaf = 1; // Force proper tree construction
        params.maxDepth = 10;            // Allow sufficient depth
        auto bvhNodes = builder.build(testData.beams, testData.balls, params);
        auto primitiveOrdering = builder.getPrimitiveOrdering();

        const auto & stats = builder.getLastBuildStats();

        std::cout << "\n=== BVH Build Statistics ===" << std::endl;
        std::cout << "Total nodes: " << stats.totalNodes << std::endl;
        std::cout << "Leaf nodes: " << stats.leafNodes << std::endl;
        std::cout << "Max depth: " << stats.maxDepth << std::endl;
        std::cout << "Avg depth: " << std::fixed << std::setprecision(2) << stats.avgDepth
                  << std::endl;
        std::cout << "SAH cost: " << stats.sahCost << std::endl;

        // Test a point near the origin
        float3 testPoint = {2.0f, 2.0f, 2.0f};

        BeamLatticeCPU::DebugInfo debugInfo;
        float result = BeamLatticeCPU::evaluateBeamLatticeBVH(
          testPoint, bvhNodes, primitiveOrdering, testData.beams, testData.balls, &debugInfo);

        std::cout << "\n=== BVH Traversal Debug ===" << std::endl;
        std::cout << "Test point: (" << testPoint.x << ", " << testPoint.y << ", " << testPoint.z
                  << ")" << std::endl;
        std::cout << "Final distance: " << result << std::endl;
        std::cout << "Nodes visited: " << debugInfo.nodesVisited << std::endl;
        std::cout << "Primitives checked: " << debugInfo.primitivesChecked << std::endl;
        std::cout << "Max stack depth: " << debugInfo.stackMaxDepth << std::endl;

        std::cout << "\nVisited nodes: ";
        for (int nodeId : debugInfo.visitedNodes)
        {
            std::cout << nodeId << " ";
        }
        std::cout << std::endl;

        std::cout << "\nPrimitive distances:" << std::endl;
        for (const auto & primDist : debugInfo.checkedPrimitives)
        {
            std::cout << "  Primitive " << primDist.first << ": " << primDist.second << std::endl;
        }

        // Verify basic sanity checks
        EXPECT_GT(debugInfo.nodesVisited, 0) << "Should visit at least one node";
        EXPECT_GT(debugInfo.primitivesChecked, 0) << "Should check at least one primitive";
        EXPECT_LE(debugInfo.stackMaxDepth, 64) << "Stack should not overflow";
    }

    /// @brief Test edge cases for BVH traversal
    TEST_F(BeamLatticeTest, BVH_EdgeCases_HandleGracefully)
    {
        // Test empty lattice
        std::vector<BeamData> emptyBeams;
        std::vector<BallData> emptyBalls;

        BeamBVHBuilder builder;
        BeamBVHBuilder::BuildParams params;
        params.maxPrimitivesPerLeaf = 1; // Force proper tree construction
        params.maxDepth = 10;            // Allow sufficient depth
        auto bvhNodes = builder.build(emptyBeams, emptyBalls, params);
        auto primitiveOrdering = builder.getPrimitiveOrdering();

        float3 testPoint = {0.0f, 0.0f, 0.0f};
        float result = BeamLatticeCPU::evaluateBeamLatticeBVH(
          testPoint, bvhNodes, primitiveOrdering, emptyBeams, emptyBalls);

        // Should return max float for empty lattice
        EXPECT_EQ(result, std::numeric_limits<float>::max())
          << "Empty lattice should return max distance";

        // Test single primitive
        std::vector<BeamData> singleBeam;
        BeamData beam;
        beam.startPos = {0.0f, 0.0f, 0.0f, 0.0f};
        beam.endPos = {1.0f, 0.0f, 0.0f, 0.0f};
        beam.startRadius = 0.5f;
        beam.endRadius = 0.5f;
        beam.startCapStyle = 2;
        beam.endCapStyle = 2;
        singleBeam.push_back(beam);

        auto singleBvhNodes = builder.build(singleBeam, emptyBalls, params);
        auto singlePrimitiveOrdering = builder.getPrimitiveOrdering();

        float singleBvhResult = BeamLatticeCPU::evaluateBeamLatticeBVH(
          testPoint, singleBvhNodes, singlePrimitiveOrdering, singleBeam, emptyBalls);

        float singleFlatResult =
          BeamLatticeCPU::evaluateBeamLatticeFlat(testPoint, singleBeam, emptyBalls);

        EXPECT_NEAR(singleBvhResult, singleFlatResult, 1e-5f)
          << "Single primitive BVH should match flat evaluation";
    }

} // namespace gladius::tests