
#include <CLMath.h>
#include <kernel/types.h>

#include "testhelper.h"

#include <Eigen/Core>
#include <Eigen/src/Core/Matrix.h>

#include <cmath>

#include <functional>
#include <gtest/gtest.h>
#include <ios>
#include <iostream>
#include <limits>
#include <random>
#include <utility>

namespace gladius_tests
{
    namespace testee
    {
        float3 rayCast(float3 pos, float3 direction, ShapeFunction model, int maxIterations)
        {
            auto startPos = pos;
            auto sdf = model(pos);
            auto constexpr tolerance = 1.E-8f;
            int i = 0;
            while ((fabs(sdf) > tolerance) && (i < maxIterations))
            {
                pos += sdf * direction;
                sdf = model(pos);
                ++i;
            }
            return pos;
        }

        float3 surfaceNormal(float3 pos, ShapeFunction model)
        {
            auto constexpr smallValue = 1.E-4f;
            auto const offset = float2{1.f, -1.f};

            float3 xyy = {offset.x(), offset.y(), offset.y()};
            float3 yyx = {offset.y(), offset.y(), offset.x()};
            float3 yxy = {offset.y(), offset.x(), offset.y()};
            float3 xxx = {offset.x(), offset.x(), offset.x()};

            float3 normal = xyy * model(pos + xyy * smallValue) + //
                            yyx * model(pos + yyx * smallValue) + //
                            yxy * model(pos + yxy * smallValue) + //
                            xxx * model(pos + xxx * smallValue);
            return normal.normalized();
        }

        float3 moveToSurface(float3 pos, ShapeFunction model)
        {
            float3 const direction = -surfaceNormal(pos, model);
            return rayCast(pos, direction, model, 1);
        }

        BoundingBox extendBB(BoundingBox bbox, float3 pos)
        {
            bbox.min.x = std::min(bbox.min.x, pos.x());
            bbox.min.y = std::min(bbox.min.y, pos.y());
            bbox.min.z = std::min(bbox.min.z, pos.z());

            bbox.max.x = std::max(bbox.max.x, pos.x());
            bbox.max.y = std::max(bbox.max.y, pos.y());
            bbox.max.z = std::max(bbox.max.z, pos.z());

            return bbox;
        }

        BoundingBox improveBoundingBox(BoundingBox bbox, ShapeFunction model)
        {

            int constexpr xSteps = 10;
            int constexpr ySteps = 10;
            int constexpr zSteps = 10;

            float xIncrement = (bbox.max.x - bbox.min.x) / static_cast<float>(xSteps);
            float yIncrement = (bbox.max.y - bbox.min.y) / static_cast<float>(ySteps);
            float zIncrement = (bbox.max.z - bbox.min.z) / static_cast<float>(zSteps);

            BoundingBox newBB{{{0.f, 0.f, 0.f}}, {{0.f, 0.f, 0.f}}};
            // Bottom and top
            for (int y = 0; y < ySteps; ++y)
            {
                for (int x = 0; x < xSteps; ++x)
                {
                    float const xf = static_cast<float>(x);
                    float const yf = static_cast<float>(y);

                    newBB = extendBB(
                      newBB,
                      moveToSurface(
                        {bbox.min.x + xf * xIncrement, bbox.min.y + yf * yIncrement, bbox.min.z},
                        model));

                    newBB = extendBB(
                      newBB,
                      moveToSurface(
                        {bbox.min.x + xf * xIncrement, bbox.min.y + yf * yIncrement, bbox.max.z},
                        model));
                }
            }

            // left and right
            for (int y = 0; y < ySteps; ++y)
            {
                for (int z = 0; z < zSteps; ++z)
                {
                    float const yf = static_cast<float>(y);
                    float const zf = static_cast<float>(z);

                    newBB = extendBB(
                      newBB,
                      moveToSurface(
                        {bbox.min.x, bbox.min.y + yf * yIncrement, bbox.min.z + zf * zIncrement},
                        model));

                    newBB = extendBB(
                      newBB,
                      moveToSurface(
                        {bbox.max.x, bbox.min.y + yf * yIncrement, bbox.min.z + zf * zIncrement},
                        model));
                }
            }

            // front and back
            for (int x = 0; x < xSteps; ++x)
            {
                for (int z = 0; z < zSteps; ++z)
                {
                    float const xf = static_cast<float>(x);
                    float const zf = static_cast<float>(z);

                    newBB = extendBB(
                      newBB,
                      moveToSurface(
                        {bbox.min.x + xf * xIncrement, bbox.min.y, bbox.min.z + zf * zIncrement},
                        model));

                    newBB = extendBB(
                      newBB,
                      moveToSurface(
                        {bbox.min.x + xf * xIncrement, bbox.max.y, bbox.min.z + zf * zIncrement},
                        model));
                }
            }
            return newBB;
        }

        BoundingBox determineBBox(ShapeFunction model, int numIterations)
        {
            auto constexpr bigNumber = 1.E3f;
            BoundingBox bbox{{{-bigNumber, -bigNumber, -bigNumber}},
                             {{bigNumber, bigNumber, bigNumber}}};
            for (int i = 0; i < numIterations; ++i)
            {
                bbox = improveBoundingBox(bbox, model);
            }
            return bbox;
        }
    }

    class BoundingBoxDetermination : public ::testing::Test
    {
    };

    TEST_F(BoundingBoxDetermination, Raycast_Sphere_PointOnSphereSurface)
    {
        ShapeFunction mediumSizedSphere = [](float3 pos) {
            auto constexpr radius = 12.34f;
            return helper::sphere(std::move(pos), radius);
        };

        auto pointOnSphere =
          testee::rayCast(float3{-50.f, 0, 0}, float3{1.f, 0, 0}, mediumSizedSphere, 1000);

        EXPECT_NEAR(mediumSizedSphere(pointOnSphere), 0.f, 1.E-3f);
    }

    TEST_F(BoundingBoxDetermination, SurfaceNormal_CenteredSphere_PointsToOrigin)
    {
        ShapeFunction mediumSizedSphere = [](float3 pos) {
            auto constexpr radius = 12.34f;
            return helper::sphere(std::move(pos), radius);
        };

        auto normal = testee::surfaceNormal(float3{-50.f, 0.f, 0.f}, mediumSizedSphere);
        EXPECT_NEAR(normal.x(), -1.0f, 1.E-4f);
        EXPECT_NEAR(normal.y(), 0.0f, 1.E-4f);
        EXPECT_NEAR(normal.z(), 0.0f, 1.E-4f);
    }

    void moveToSurfaceTest(float3 pos)
    {
        ShapeFunction mediumSizedSphere = [](float3 pos) {
            auto constexpr radius = 12.34f;
            return helper::sphere(std::move(pos), radius);
        };
        auto movedPoint = testee::moveToSurface(pos, mediumSizedSphere);

        auto const sdfAtPos = mediumSizedSphere(pos);
        auto const sdfAtMovedPoint = mediumSizedSphere(movedPoint);
        EXPECT_LT(fabs(sdfAtMovedPoint), fabs(sdfAtPos));
    }

    TEST_F(BoundingBoxDetermination, MoveToSurface_CenteredSphere_ReturnsPointCloserToSurface)
    {
        moveToSurfaceTest({-50.f, 0.f, 0.f});
        moveToSurfaceTest({50.f, 0.f, 0.f});
        moveToSurfaceTest({-50.f, 50.f, 0.f});
        moveToSurfaceTest({0.f, 50.f, 0.f});
        moveToSurfaceTest({-50.f, 0.f, 50.f});
        moveToSurfaceTest({0.f, 0.f, 50.f});
        moveToSurfaceTest({0.f, 0.f, 0.1f});
    }

    TEST_F(BoundingBoxDetermination, ExtendBB_EmptyBBByAbritaryPoint_ReturnsExtendedBBox)
    {
        BoundingBox bbox{{{0.f, 0.f, 0.f}}, {{0.f, 0.f, 0.f}}};

        auto extendedBBox = testee::extendBB(bbox, {123.f, 456.f, 789.f});
        EXPECT_EQ(extendedBBox.min.x, 0.f);
        EXPECT_EQ(extendedBBox.min.y, 0.f);
        EXPECT_EQ(extendedBBox.min.z, 0.f);

        EXPECT_EQ(extendedBBox.max.x, 123.f);
        EXPECT_EQ(extendedBBox.max.y, 456.f);
        EXPECT_EQ(extendedBBox.max.z, 789.f);

        extendedBBox = testee::extendBB(extendedBBox, {-123.f, -456.f, -789.f});

        EXPECT_EQ(extendedBBox.min.x, -123.f);
        EXPECT_EQ(extendedBBox.min.y, -456.f);
        EXPECT_EQ(extendedBBox.min.z, -789.f);
    }

    TEST_F(BoundingBoxDetermination, DetermineBoundingBox_Sphere_BoxDimensionsEqualDiameter)
    {
        auto constexpr radius = 12.34f;
        ShapeFunction mediumSizedSphere = [&radius](float3 pos) {
            return helper::sphere(std::move(pos), radius);
        };

        auto bbox = testee::determineBBox(mediumSizedSphere, 10);
        auto width = bbox.max.x - bbox.min.x;
        auto length = bbox.max.y - bbox.min.y;
        auto height = bbox.max.z - bbox.min.z;
        auto const diameter = 2. * radius;
        EXPECT_NEAR(width, diameter, 1.E-4f);
        EXPECT_NEAR(length, diameter, 1.E-4f);
        EXPECT_NEAR(height, diameter, 1.E-4f);
    }
}