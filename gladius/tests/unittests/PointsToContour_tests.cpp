

#include <contour/PointsToContour.h>
#include <contour/QuadTree.h>

#include <gtest/gtest.h>

namespace gladius_tests
{
    using namespace gladius::contour;

    TEST(PointsToContour,
         ConvertToPolylines_PointCloudContainigQuad_ReturnsOneClosedPolylineWithAllVertices)
    {

        auto const domain = Rect{{0.f, 0.f}, {1000.f, 1000.f}};
        QuadTree quadTree{domain};

        PointWithNormal const a{{10.f, 10.f}, {-1.f, -1.f}}; 
        PointWithNormal const b{{20.f, 10.f}, {1.f, -1.f}};
        PointWithNormal const c{{20.f, 20.f}, {1.f, 1.f}};
        PointWithNormal const d{{10.f, 20.f}, {-1.f, 1.f}};

        quadTree.insert(a);
        quadTree.insert(b);
        quadTree.insert(c);
        quadTree.insert(d);

        auto polyLines = convertToPolylines(quadTree, 11.f);

        EXPECT_EQ(polyLines.size(), 1);
        EXPECT_EQ(polyLines.front().vertices.size(), 4);
    }

    TEST(PointsToContour,
         ConvertToPolylines_PointCloudContainigTwoSeperateQuads_ReturnsTwoClosedPolylines)
    {

        auto const domain = Rect{{0.f, 0.f}, {1000.f, 1000.f}};
        QuadTree quadTree{domain};

        quadTree.insert({{10.f, 10.f}, {-1.f, -1.f}});
        quadTree.insert({{20.f, 10.f}, {1.f, -1.f}});
        quadTree.insert({{20.f, 20.f}, {1.f, 1.f}});
        quadTree.insert({{10.f, 20.f}, {-1.f, 1.f}});

        quadTree.insert({{50.f + 10.f, 50.f + 10.f}, {-1.f, -1.f}});
        quadTree.insert({{50.f + 20.f, 50.f + 10.f}, {1.f, -1.f}});
        quadTree.insert({{50.f + 20.f, 50.f + 20.f}, {1.f, 1.f}});
        quadTree.insert({{50.f + 10.f, 50.f + 20.f}, {-1.f, 1.f}});

        auto polyLines = convertToPolylines(quadTree, 11.f);

        EXPECT_EQ(polyLines.size(), 2);
        EXPECT_EQ(polyLines.front().vertices.size(), 4);
        EXPECT_EQ(polyLines.back().vertices.size(), 4);
    }

}