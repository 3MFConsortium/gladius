#include "ContourExtractor.h"
#include "PathBuilder.h"
#include "contour/utils.h"
#include <Contour.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <memory>

namespace gladius_tests
{

    /// @brief Dummy logger that satisfies the SharedLogger requirements.
    /// For our tests we do not need real logging.
    class DummyLogger : public gladius::events::Logger
    {
      public:
        void addEvent(const gladius::events::Event & /*event*/) override
        {
        }
    };

    /// @brief Creates a dummy logger for testing purposes.
    /// @return Shared pointer to a dummy logger instance.
    std::shared_ptr<gladius::events::Logger> makeDummyLogger()
    {
        return std::make_shared<DummyLogger>();
    }

    /// @brief Helper to create a closed PolyLine from a list of points.
    /// @param pts Vector of points defining the polygon vertices
    /// @return A closed PolyLine with calculated area
    gladius::PolyLine createClosedPolyLine(const std::vector<gladius::Vector2> & pts)
    {
        gladius::PolyLine poly;
        poly.vertices = pts;
        // Ensure the polyline is closed. If not, force it to be.
        if (pts.front() != pts.back())
        {
            poly.vertices.push_back(pts.front());
        }
        poly.isClosed = true;
        // Calculate area using the shoelace formula.
        poly.area = gladius::calcArea(poly);
        return poly;
    }

    /// @brief Creates a simple square polyline for testing.
    /// @param size Side length of the square
    /// @param offset Offset from origin
    /// @return A square PolyLine
    gladius::PolyLine createSquare(float size, gladius::Vector2 offset = gladius::Vector2(0.f, 0.f))
    {
        std::vector<gladius::Vector2> pts = {offset + gladius::Vector2(0.f, 0.f),
                                             offset + gladius::Vector2(size, 0.f),
                                             offset + gladius::Vector2(size, size),
                                             offset + gladius::Vector2(0.f, size)};
        return createClosedPolyLine(pts);
    }

    /// @brief Creates a triangle polyline for testing.
    /// @param size Base size of the triangle
    /// @param offset Offset from origin
    /// @return A triangle PolyLine
    gladius::PolyLine createTriangle(float size,
                                     gladius::Vector2 offset = gladius::Vector2(0.f, 0.f))
    {
        std::vector<gladius::Vector2> pts = {offset + gladius::Vector2(0.f, 0.f),
                                             offset + gladius::Vector2(size, 0.f),
                                             offset + gladius::Vector2(size / 2.f, size)};
        return createClosedPolyLine(pts);
    }

    /// @brief Test fixture for ContourExtractor tests
    class ContourExtractorTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            m_logger = makeDummyLogger();
            m_extractor = std::make_unique<gladius::ContourExtractor>(m_logger);
        }

        void TearDown() override
        {
            m_extractor.reset();
        }

        /// @brief Helper to access and modify contours directly
        std::vector<gladius::PolyLine> & getContours()
        {
            return const_cast<std::vector<gladius::PolyLine> &>(m_extractor->getContour());
        }

        /// @brief Helper to setup contours and run calcSign
        void setupContoursAndCalcSign(const std::vector<gladius::PolyLine> & contours)
        {
            auto & extractorContours = getContours();
            extractorContours.clear();
            extractorContours = contours;
            m_extractor->calcAreas();
            m_extractor->calcSign();
        }

        std::shared_ptr<gladius::events::Logger> m_logger;
        std::unique_ptr<gladius::ContourExtractor> m_extractor;
    };

    // Test basic functionality - calcSign with a single outer contour and one inner contour (hole)
    TEST_F(ContourExtractorTest, CalcSign_OuterAndInnerContours_AssignsCorrectSigns)
    {
        // Outer contour: a square from (0,0) to (10,10)
        auto outer = createSquare(10.f);

        // Inner contour: a square from (3,3) to (7,7) inside the outer contour.
        auto inner = createSquare(4.f, gladius::Vector2(3.f, 3.f));

        setupContoursAndCalcSign({outer, inner});

        const auto & resultContours = m_extractor->getContour();
        ASSERT_EQ(resultContours.size(), 2);

        // The outer contour should have positive area (even number of containing contours: 0)
        // The inner contour should have negative area (odd number of containing contours: 1)
        int positiveCount = 0, negativeCount = 0;
        for (const auto & poly : resultContours)
        {
            if (poly.area > 0.f)
                positiveCount++;
            else if (poly.area < 0.f)
                negativeCount++;
        }

        EXPECT_EQ(positiveCount, 1);
        EXPECT_EQ(negativeCount, 1);
    }

    // Test multiple nested contours: outer with two holes
    TEST_F(ContourExtractorTest, CalcSign_MultipleNestedContours_AssignsCorrectSigns)
    {
        // Outer contour: square from (0,0) to (20,20)
        auto outer = createSquare(20.f);

        // First hole: square from (2,2) to (8,8)
        auto hole1 = createSquare(6.f, gladius::Vector2(2.f, 2.f));

        // Second hole: square from (12,12) to (18,18)
        auto hole2 = createSquare(6.f, gladius::Vector2(12.f, 12.f));

        setupContoursAndCalcSign({outer, hole1, hole2});

        const auto & resultContours = m_extractor->getContour();
        ASSERT_EQ(resultContours.size(), 3);

        // Assert the expected signs for each contour
        EXPECT_GT(resultContours[0].area, 0.f); // outer contour
        EXPECT_LT(resultContours[1].area, 0.f); // first hole
        EXPECT_LT(resultContours[2].area, 0.f); // second hole
    }

    // Test degenerate contour: very few vertices or almost a line
    TEST_F(ContourExtractorTest, CalcSign_DegenerateContour_DoesNotCrash)
    {
        // Create a degenerate contour: three collinear points
        std::vector<gladius::Vector2> degeneratePts = {gladius::Vector2(0.f, 0.f),
                                                       gladius::Vector2(5.f, 5.f),
                                                       gladius::Vector2(10.f, 10.f),
                                                       gladius::Vector2(0.f, 0.f)};
        auto degeneratePoly = createClosedPolyLine(degeneratePts);

        // Even if the computed area is zero (or near zero) the method should not crash
        EXPECT_NEAR(degeneratePoly.area, 0.f, 1e-5f);

        setupContoursAndCalcSign({degeneratePoly});

        // Running calcSign should not crash though the contour is degenerate
        EXPECT_NO_THROW(m_extractor->calcSign());

        // Since it is degenerate, we expect the absolute area is zero
        EXPECT_NEAR(m_extractor->getContour().front().area, 0.f, 1e-5f);
    }

    // Test complex nesting: outer -> hole -> inner shape
    TEST_F(ContourExtractorTest, CalcSign_ComplexNesting_AssignsCorrectSigns)
    {
        // Outer boundary: large square
        auto outer = createSquare(50.f);

        // Hole in outer: medium square
        auto hole = createSquare(30.f, gladius::Vector2(10.f, 10.f));

        // Shape inside hole: small square
        auto innerShape = createSquare(10.f, gladius::Vector2(20.f, 20.f));

        setupContoursAndCalcSign({outer, hole, innerShape});

        const auto & resultContours = m_extractor->getContour();
        ASSERT_EQ(resultContours.size(), 3);

        EXPECT_GT(resultContours[0].area, 0.f); // outer: 0 containing -> positive
        EXPECT_LT(resultContours[1].area, 0.f); // hole: 1 containing -> negative
        EXPECT_GT(resultContours[2].area, 0.f); // inner: 2 containing -> positive
    }

    // Test empty contours list
    TEST_F(ContourExtractorTest, CalcSign_EmptyContours_DoesNotCrash)
    {
        setupContoursAndCalcSign({});

        EXPECT_NO_THROW(m_extractor->calcSign());
        EXPECT_TRUE(m_extractor->getContour().empty());
    }

    // Test single contour
    TEST_F(ContourExtractorTest, CalcSign_SingleContour_RemainsPositive)
    {
        auto single = createSquare(10.f);

        setupContoursAndCalcSign({single});

        const auto & resultContours = m_extractor->getContour();
        ASSERT_EQ(resultContours.size(), 1);
        EXPECT_GT(resultContours[0].area, 0.f); // No containing contours -> positive
    }

    // Test contours with zero area
    TEST_F(ContourExtractorTest, CalcSign_ZeroAreaContour_HandledCorrectly)
    {
        gladius::PolyLine zeroAreaPoly;
        zeroAreaPoly.vertices = {
          gladius::Vector2(0.f, 0.f), gladius::Vector2(0.f, 0.f), gladius::Vector2(0.f, 0.f)};
        zeroAreaPoly.isClosed = true;
        zeroAreaPoly.area = 0.f;

        setupContoursAndCalcSign({zeroAreaPoly});

        EXPECT_NO_THROW(m_extractor->calcSign());
        EXPECT_NEAR(m_extractor->getContour().front().area, 0.f, 1e-6f);
    }

    // Test area calculation accuracy
    TEST_F(ContourExtractorTest, CalcAreas_VariousShapes_CalculatesCorrectAreas)
    {
        // Test square area calculation
        auto square = createSquare(10.f);
        auto & contours = getContours();
        contours.clear();
        contours.push_back(square);

        m_extractor->calcAreas();

        const auto & result = m_extractor->getContour();
        ASSERT_EQ(result.size(), 1);
        EXPECT_NEAR(std::abs(result[0].area), 100.f, 1e-5f); // 10x10 = 100

        // Test triangle area calculation
        contours.clear();
        auto triangle = createTriangle(10.f);
        contours.push_back(triangle);

        m_extractor->calcAreas();

        const auto & triangleResult = m_extractor->getContour();
        ASSERT_EQ(triangleResult.size(), 1);
        EXPECT_NEAR(std::abs(triangleResult[0].area),
                    50.f,
                    1e-5f); // 0.5 * base * height = 0.5 * 10 * 10 = 50
    }

    // Test simplification tolerance
    TEST_F(ContourExtractorTest, SetSimplificationTolerance_ValidValue_SetsCorrectly)
    {
        constexpr float TOLERANCE = 0.1f;

        EXPECT_NO_THROW(m_extractor->setSimplificationTolerance(TOLERANCE));

        // Test that simplification doesn't crash with various tolerance values
        EXPECT_NO_THROW(m_extractor->setSimplificationTolerance(0.0f));
        EXPECT_NO_THROW(m_extractor->setSimplificationTolerance(1.0f));
    }

    // Test contour clearing
    TEST_F(ContourExtractorTest, Clear_WithContours_RemovesAllContours)
    {
        auto square = createSquare(10.f);
        auto & contours = getContours();
        contours.push_back(square);

        ASSERT_FALSE(contours.empty());

        m_extractor->clear();

        EXPECT_TRUE(m_extractor->getContour().empty());
        EXPECT_TRUE(m_extractor->getOpenContours().empty());
    }

    // Test post-processing pipeline
    TEST_F(ContourExtractorTest, RunPostProcessing_WithValidContours_CompletesSuccessfully)
    {
        auto outer = createSquare(20.f);
        auto inner = createSquare(10.f, gladius::Vector2(5.f, 5.f));

        auto & contours = getContours();
        contours.clear();
        contours.insert(contours.end(), {outer, inner});

        EXPECT_NO_THROW(m_extractor->runPostProcessing());

        // Verify areas are calculated and signs are assigned
        const auto & result = m_extractor->getContour();
        for (const auto & contour : result)
        {
            EXPECT_NE(contour.area, 0.f); // Area should be calculated
        }
    }

    // Test slice quality metrics
    TEST_F(ContourExtractorTest, GetSliceQuality_AfterProcessing_ProvidesValidMetrics)
    {
        auto square = createSquare(10.f);
        auto & contours = getContours();
        contours.push_back(square);

        m_extractor->runPostProcessing();

        const auto & quality = m_extractor->getSliceQuality();

        EXPECT_GE(quality.closedPolyLines, 0u);
        EXPECT_GE(quality.selfIntersections, 0u);
        EXPECT_GE(quality.enclosedArea, 0.f);
    }

    // Test edge case: contour with only two vertices
    TEST_F(ContourExtractorTest, CalcSign_TwoVertexContour_HandledGracefully)
    {
        gladius::PolyLine twoVertexPoly;
        twoVertexPoly.vertices = {gladius::Vector2(0.f, 0.f), gladius::Vector2(1.f, 1.f)};
        twoVertexPoly.isClosed = false; // Two vertices cannot form a closed shape
        twoVertexPoly.area = 0.f;

        auto & contours = getContours();
        contours.clear();
        contours.push_back(twoVertexPoly);

        EXPECT_NO_THROW(m_extractor->calcSign());
    }

    // Performance and stress tests
    TEST_F(ContourExtractorTest, CalcSign_LargeNumberOfContours_PerformsReasonably)
    {
        // Create many contours to test performance
        std::vector<gladius::PolyLine> manyContours;
        constexpr int NUM_CONTOURS = 100;

        for (int i = 0; i < NUM_CONTOURS; ++i)
        {
            auto square = createSquare(2.f, gladius::Vector2(static_cast<float>(i * 3), 0.f));
            manyContours.push_back(square);
        }

        auto start = std::chrono::high_resolution_clock::now();
        setupContoursAndCalcSign(manyContours);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // Performance should be reasonable (less than 1 second for 100 contours)
        EXPECT_LT(duration.count(), 1000);

        const auto & result = m_extractor->getContour();
        EXPECT_EQ(result.size(), NUM_CONTOURS);

        // All should be positive (no nesting)
        for (const auto & contour : result)
        {
            EXPECT_GT(contour.area, 0.f);
        }
    }

    // Test with very small contours (precision edge case)
    TEST_F(ContourExtractorTest, CalcSign_VerySmallContours_HandledCorrectly)
    {
        constexpr float TINY_SIZE = 1e-6f;
        auto tinySquare = createSquare(TINY_SIZE);

        setupContoursAndCalcSign({tinySquare});

        const auto & result = m_extractor->getContour();
        ASSERT_EQ(result.size(), 1);
        EXPECT_GT(result[0].area, 0.f);
        EXPECT_NEAR(result[0].area, TINY_SIZE * TINY_SIZE, 1e-12f);
    }

    // Test with very large contours
    TEST_F(ContourExtractorTest, CalcSign_VeryLargeContours_HandledCorrectly)
    {
        constexpr float LARGE_SIZE = 1e6f;
        auto largeSquare = createSquare(LARGE_SIZE);

        setupContoursAndCalcSign({largeSquare});

        const auto & result = m_extractor->getContour();
        ASSERT_EQ(result.size(), 1);
        EXPECT_GT(result[0].area, 0.f);
        EXPECT_NEAR(result[0].area, LARGE_SIZE * LARGE_SIZE, 1e6f);
    }

    // Test concentric circles approximation using many-sided polygons
    TEST_F(ContourExtractorTest, CalcSign_ConcentricPolygons_AssignsAlternatingSigns)
    {
        // Create concentric "circles" using high-vertex polygons
        auto createRegularPolygon =
          [](float radius, gladius::Vector2 center, int vertices) -> gladius::PolyLine
        {
            std::vector<gladius::Vector2> pts;
            constexpr float PI = 3.14159265359f;

            for (int i = 0; i < vertices; ++i)
            {
                float angle = 2.f * PI * static_cast<float>(i) / static_cast<float>(vertices);
                pts.push_back(center +
                              gladius::Vector2(radius * std::cos(angle), radius * std::sin(angle)));
            }

            return createClosedPolyLine(pts);
        };

        gladius::Vector2 center(50.f, 50.f);
        auto outer = createRegularPolygon(30.f, center, 20);
        auto middle = createRegularPolygon(20.f, center, 16);
        auto inner = createRegularPolygon(10.f, center, 12);

        setupContoursAndCalcSign({outer, middle, inner});

        const auto & result = m_extractor->getContour();
        ASSERT_EQ(result.size(), 3);

        EXPECT_GT(result[0].area, 0.f); // outer: 0 containing -> positive
        EXPECT_LT(result[1].area, 0.f); // middle: 1 containing -> negative
        EXPECT_GT(result[2].area, 0.f); // inner: 2 containing -> positive
    }

    // Test contours at boundary conditions
    TEST_F(ContourExtractorTest, CalcSign_ContourOnBoundary_HandledCorrectly)
    {
        // Create two squares that touch at an edge
        auto square1 = createSquare(10.f, gladius::Vector2(0.f, 0.f));
        auto square2 = createSquare(10.f, gladius::Vector2(10.f, 0.f)); // Touches first square

        setupContoursAndCalcSign({square1, square2});

        const auto & result = m_extractor->getContour();
        ASSERT_EQ(result.size(), 2);

        // Both should be positive (no containment)
        EXPECT_GT(result[0].area, 0.f);
        EXPECT_GT(result[1].area, 0.f);
    }

    // Test invalid contour data handling
    TEST_F(ContourExtractorTest, CalcSign_ContourWithNaNVertices_HandledGracefully)
    {
        gladius::PolyLine invalidPoly;
        invalidPoly.vertices = {gladius::Vector2(0.f, 0.f),
                                gladius::Vector2(std::numeric_limits<float>::quiet_NaN(), 0.f),
                                gladius::Vector2(1.f, 1.f)};
        invalidPoly.isClosed = true;
        invalidPoly.area = 0.f;

        auto & contours = getContours();
        contours.clear();
        contours.push_back(invalidPoly);

        // Should not crash even with invalid data
        EXPECT_NO_THROW(m_extractor->calcSign());
    }

    // Test offset contour generation
    TEST_F(ContourExtractorTest, GenerateOffsetContours_PositiveOffset_CreatesLargerContours)
    {
        auto square = createSquare(10.f);
        constexpr float OFFSET = 2.f;

        auto offsetContours = m_extractor->generateOffsetContours(OFFSET, {square});

        ASSERT_FALSE(offsetContours.empty());

        // Calculate areas for comparison since generateOffsetContours doesn't calculate them
        for (auto & offsetContour : offsetContours)
        {
            offsetContour.area = gladius::calcArea(offsetContour);
        }

        // Offset contours should have larger area than original
        for (const auto & offsetContour : offsetContours)
        {
            EXPECT_GT(std::abs(offsetContour.area), std::abs(square.area));
        }
    }

    // Test negative offset (inward)
    TEST_F(ContourExtractorTest, GenerateOffsetContours_NegativeOffset_CreatesSmallerContours)
    {
        auto square = createSquare(20.f); // Large enough to handle negative offset
        constexpr float OFFSET = -2.f;

        auto offsetContours = m_extractor->generateOffsetContours(OFFSET, {square});

        ASSERT_FALSE(offsetContours.empty());

        // Calculate areas for comparison since generateOffsetContours doesn't calculate them
        for (auto & offsetContour : offsetContours)
        {
            offsetContour.area = gladius::calcArea(offsetContour);
        }

        // Offset contours should have smaller area than original
        for (const auto & offsetContour : offsetContours)
        {
            EXPECT_LT(std::abs(offsetContour.area), std::abs(square.area));
        }
    }

    // Test zero offset
    TEST_F(ContourExtractorTest, GenerateOffsetContours_ZeroOffset_ReturnsOriginalContours)
    {
        auto square = createSquare(10.f);
        constexpr float OFFSET = 0.f;

        auto offsetContours = m_extractor->generateOffsetContours(OFFSET, {square});

        ASSERT_EQ(offsetContours.size(), 1);
        EXPECT_NEAR(offsetContours[0].area, square.area, 1e-6f);
    }

    // Test thread safety (basic check)
    TEST_F(ContourExtractorTest, CalcSign_MultipleExtractors_WorkIndependently)
    {
        auto logger1 = makeDummyLogger();
        auto logger2 = makeDummyLogger();

        gladius::ContourExtractor extractor1(logger1);
        gladius::ContourExtractor extractor2(logger2);

        auto square1 = createSquare(10.f);
        auto square2 = createSquare(20.f);

        // Setup different contours in each extractor
        auto & contours1 = const_cast<std::vector<gladius::PolyLine> &>(extractor1.getContour());
        auto & contours2 = const_cast<std::vector<gladius::PolyLine> &>(extractor2.getContour());

        contours1.clear();
        contours1.push_back(square1);
        extractor1.calcAreas();

        contours2.clear();
        contours2.push_back(square2);
        extractor2.calcAreas();

        // Both should work independently
        EXPECT_NO_THROW(extractor1.calcSign());
        EXPECT_NO_THROW(extractor2.calcSign());

        EXPECT_NEAR(std::abs(extractor1.getContour()[0].area), 100.f, 1e-5f);
        EXPECT_NEAR(std::abs(extractor2.getContour()[0].area), 400.f, 1e-5f);
    }

} // namespace gladius_tests