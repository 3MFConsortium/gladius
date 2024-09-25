

#include <contour/QuadTree.h>

#include <gtest/gtest.h>

namespace gladius_tests
{
    using namespace gladius::contour;

    TEST(Quad, Insert_PointInsideBoundaries_ReturnsTrue)
    {
        Quad rootQuad{Rect{{0.f, 0.f}, {100.f, 100.f}}, nullptr};

        PointWithNormal const inside{gladius::Vector2{12.3f, 34.f}, {}};
        EXPECT_TRUE(rootQuad.insert(inside));
    }

    TEST(Quad, Insert_PointOutSideBoundaries_ReturnsFalse)
    {
        Quad rootQuad{Rect{{0.f, 0.f}, {100.f, 100.f}}, nullptr};

        PointWithNormal const outside{gladius::Vector2{123.f, 34.f}, {}};
        EXPECT_FALSE(rootQuad.insert(outside));
    }

    TEST(QuadTree, FindNearestNeighbor_TwoPoints_ReturnsOtherPoint)
    {
        auto const domain = Rect{{0.f, 0.f}, {1000.f, 1000.f}};
        QuadTree quadTree{domain};

        PointWithNormal const firstPoint{{5.f, 123.5f}, {1.0f, 0.f}};
        PointWithNormal const secondPoint{{15.f, 123.5f}, {1.0f, 0.f}};

        EXPECT_FALSE(quadTree.findNearestNeighbor(firstPoint.position).has_value());
        quadTree.insert(firstPoint);
        EXPECT_FALSE(quadTree.findNearestNeighbor(firstPoint.position).has_value());

        quadTree.insert(secondPoint);
        EXPECT_TRUE(quadTree.findNearestNeighbor(firstPoint.position).has_value());
    }

    TEST(QuadTree, Find_PointOutSideDomain_ReturnsEmpty)
    {
        auto const domain = Rect{{0.f, 0.f}, {1000.f, 1000.f}};
        QuadTree quadTree{domain};

        quadTree.insert({{15.f, 13.5f}, {1.0f, 0.f}});
        quadTree.insert({{15.f, 113.5f}, {1.0f, 0.f}});
        quadTree.insert({{25.f, 3.5f}, {1.0f, 0.f}});
        quadTree.insert({{535.f, 823.5f}, {1.0f, 0.f}});
        quadTree.insert({{142.f, 73.5f}, {1.0f, 0.f}});

        auto const finding = quadTree.find({1200.f, 10.f});

        EXPECT_FALSE(finding.has_value());
    }

    TEST(QuadTree, Find_PointInSideDomain_ReturnsEmpty)
    {
        auto const domain = Rect{{0.f, 0.f}, {1000.f, 1000.f}};
        QuadTree quadTree{domain};

        quadTree.insert({{15.f, 13.5f}, {1.0f, 0.f}});
        quadTree.insert({{15.f, 113.5f}, {1.0f, 0.f}});
        quadTree.insert({{25.f, 3.5f}, {1.0f, 0.f}});
        quadTree.insert({{535.f, 823.5f}, {1.0f, 0.f}});
        quadTree.insert({{142.f, 73.5f}, {1.0f, 0.f}});

        auto const finding = quadTree.find({200.f, 10.f});

        EXPECT_TRUE(finding.has_value());
    }

    TEST(QuadTree, FindNeighbors_MaxRange_ReturnsAllPoints)
    {
        auto const domain = Rect{{0.f, 0.f}, {1000.f, 1000.f}};
        QuadTree quadTree{domain};

        quadTree.insert({{15.f, 13.5f}, {1.0f, 0.f}});
        quadTree.insert({{15.f, 113.5f}, {1.0f, 0.f}});
        quadTree.insert({{25.f, 3.5f}, {1.0f, 0.f}});
        quadTree.insert({{535.f, 823.5f}, {1.0f, 0.f}});
        quadTree.insert({{142.f, 73.5f}, {1.0f, 0.f}});

        auto const neighbors =
          quadTree.findNeighbors({500.f, 500.f}, std::numeric_limits<float>::max());

        EXPECT_EQ(neighbors.size(), 5);
    }

    TEST(QuadTree, FindNeighbors_LimitedRange_ReturnsOnlyPointInRange)
    {
        auto const domain = Rect{{0.f, 0.f}, {1000.f, 1000.f}};
        QuadTree quadTree{domain};

        auto const pointInRange = PointWithNormal{{15.f, 15.5f}, {1.0f, 0.f}};

        quadTree.insert({{15.f, 113.5f}, {1.0f, 0.f}});
        quadTree.insert({{25.f, 3.5f}, {1.0f, 0.f}});
        quadTree.insert(pointInRange);
        quadTree.insert({{535.f, 823.5f}, {1.0f, 0.f}});
        quadTree.insert({{142.f, 73.5f}, {1.0f, 0.f}});

        auto constexpr maxDistance = 2.f;
        auto const neighbors = quadTree.findNeighbors({16.f, 16.f}, maxDistance);

        EXPECT_EQ(neighbors.size(), 1);

        EXPECT_EQ(neighbors.front().position, pointInRange.position);
    }

    TEST(QuadTree, RemovePoint_ExistingPoint_SizeDecreasesByOne)
    {
        auto const domain = Rect{{0.f, 0.f}, {1000.f, 1000.f}};
        QuadTree quadTree{domain};

        auto const pointToBeRemoved = PointWithNormal{{15.f, 15.5f}, {1.0f, 0.f}};

        quadTree.insert({{15.f, 113.5f}, {1.0f, 0.f}});
        quadTree.insert({{25.f, 3.5f}, {1.0f, 0.f}});
        quadTree.insert(pointToBeRemoved);
        quadTree.insert({{535.f, 823.5f}, {1.0f, 0.f}});
        quadTree.insert({{142.f, 73.5f}, {1.0f, 0.f}});

        auto const neighbors =
          quadTree.findNeighbors({500.f, 500.f}, std::numeric_limits<float>::max());

        EXPECT_EQ(neighbors.size(), 5);

        quadTree.remove(pointToBeRemoved);

        auto const neighborsAfterRemoval =
          quadTree.findNeighbors({500.f, 500.f}, std::numeric_limits<float>::max());

        EXPECT_EQ(neighborsAfterRemoval.size(), 4);
    }

}