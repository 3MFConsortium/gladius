#include <Contour.h>
#include "contour/ContourValidator.h"

#include <gtest/gtest.h>

#include "contour/utils.h"

namespace gladius_tests
{
    using namespace gladius;

    PolyLine createValidContour()
    {
        PolyLine shape;

        //Quad
        shape.vertices.push_back({0., 0.});
        shape.vertices.push_back({5., 0.});
        shape.vertices.push_back({5., 5.});
        shape.vertices.push_back({0., 5.});
        shape.vertices.push_back({0., 0.});
        return shape;
    }

    PolyLine createShapeWithSelfIntersection()
    {
        PolyLine shape;
        
        shape.vertices.push_back({0., 0.});
        shape.vertices.push_back({5., 0.});
        shape.vertices.push_back({5., 5.});
        shape.vertices.push_back({6., 4.});
        shape.vertices.push_back({0., 5.});
        shape.vertices.push_back({0., 0.});
        return shape;
    }

    TEST(ContourValidator, Validate_ValidContour_ReturnsIsValid)
    {
        // arrange
        auto validShape = createValidContour();

        //act
        auto const result = contour::validate(validShape);

        //assert
        EXPECT_TRUE(result.intersectionFree);
    }


    TEST(ContourValidator, IntersectionOfTwoLineSegments_ParallelLines_ResultIsEmpty)
    {
        auto const intersection =
            contour::utils::intersectionOfTwoLineSegments({0., 0.}, {0., 10.}, {5., 0.}, {5., 20.});

        EXPECT_FALSE(intersection.has_value());
    }

    TEST(ContourValidator, IntersectionOfTwoLineSegments_CrossingLines_ReturnsIntersection)
    {
        auto const intersection =
            contour::utils::intersectionOfTwoLineSegments({3., 0.}, {3., 10.}, {0., 2.}, {5., 2.});



        EXPECT_TRUE(intersection.has_value());

        EXPECT_NEAR(intersection->x(), 3., 1.E-7);
        EXPECT_NEAR(intersection->y(), 2., 1.E-7);

    }

	TEST(ContourValidator, Validate_SelfintersectingContour_ReturnsIsInvalid)
    {
        // arrange
        auto invalidShape = createShapeWithSelfIntersection();

        // act
        auto const result = contour::validate(invalidShape);

        // assert
        EXPECT_FALSE(result.intersectionFree);
    }
}
