#include <Contour.h>
#include "ContourExtractor.h"
#include "contour/utils.h"

#include <gtest/gtest.h>
#include <memory>
#include <cmath>

namespace gladius_tests {

// Dummy logger that satisfies the SharedLogger requirements.
// For our tests we do not need real logging.
class DummyLogger : public gladius::events::Logger
{
public:
    void addEvent(const gladius::events::Event & /*event*/) override {}
};

std::shared_ptr<gladius::events::Logger> makeDummyLogger() {
    return std::make_shared<DummyLogger>();
}

// Helper to create a closed PolyLine from a list of points.
gladius::PolyLine createClosedPolyLine(const std::vector<gladius::Vector2>& pts)
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
    float area = 0.f;
    const size_t n = poly.vertices.size();
    for (size_t i = 0; i < n - 1; ++i)
    {
        const auto &p0 = poly.vertices[i];
        const auto &p1 = poly.vertices[i + 1];
        area += p0.x() * p1.y() - p1.x() * p0.y();
    }
    poly.area = std::fabs(area) * 0.5f; // store as absolute area before sign correction
    return poly;
}

// Test calcSign with a single outer contour and one inner contour (hole)
TEST(ContourExtractorTest, OuterAndInnerContours)
{
    // Outer contour: a square from (0,0) to (10,10)
    std::vector<gladius::Vector2> outerPts = {
        gladius::Vector2(0.f, 0.f),
        gladius::Vector2(10.f, 0.f),
        gladius::Vector2(10.f, 10.f),
        gladius::Vector2(0.f, 10.f),
        gladius::Vector2(0.f, 0.f)
    };
    // Inner contour: a square from (3,3) to (7,7) inside the outer contour.
    std::vector<gladius::Vector2> innerPts = {
        gladius::Vector2(3.f, 3.f),
        gladius::Vector2(7.f, 3.f),
        gladius::Vector2(7.f, 7.f),
        gladius::Vector2(3.f, 7.f),
        gladius::Vector2(3.f, 3.f)
    };
    
    gladius::PolyLine outer = createClosedPolyLine(outerPts);
    gladius::PolyLine inner = createClosedPolyLine(innerPts);
    
    // Initially, both areas are stored as positive.
    EXPECT_GT(outer.area, 0.f);
    EXPECT_GT(inner.area, 0.f);
    
    // Create a ContourExtractor and populate its m_closedContours.
    gladius::ContourExtractor extractor(makeDummyLogger());
    // Note: we assume m_closedContours is accessible indirectly via getContour().
    auto &contours = const_cast<std::vector<gladius::PolyLine>&>(extractor.getContour());
    contours.clear();
    contours.push_back(outer);
    contours.push_back(inner);
    
    // Run calcSign (which internally computes centroids and counts containment).
    extractor.calcAreas();
    extractor.calcSign();
    
    // The outer contour should have an even number of surrounding contours (0) → positive.
    // The inner contour is inside the outer one → negative.
    const auto &resultContours = extractor.getContour();
    // Find the contours by checking centroid positions.
    int positiveCount = 0, negativeCount = 0;
    for (const auto &poly : resultContours)
    {
        if (poly.area > 0.f)
            positiveCount++;
        else if (poly.area < 0.f)
            negativeCount++;
    }
    
    EXPECT_EQ(positiveCount, 1);
    EXPECT_EQ(negativeCount, 1);
}

// Test multiple nested contours. This constructs an outer contour with two nested holes.
// The outer should be positive; the holes (level 1) negative.
TEST(ContourExtractorTest, MultipleNestedContours)
{
    // Outer contour: square from (0,0) to (20,20)
    std::vector<gladius::Vector2> outerPts = {
        gladius::Vector2(0.f, 0.f),
        gladius::Vector2(20.f, 0.f),
        gladius::Vector2(20.f, 20.f),
        gladius::Vector2(0.f, 20.f),
        gladius::Vector2(0.f, 0.f)
    };
    // First hole: square from (2,2) to (8,8)
    std::vector<gladius::Vector2> hole1Pts = {
        gladius::Vector2(2.f, 2.f),
        gladius::Vector2(8.f, 2.f),
        gladius::Vector2(8.f, 8.f),
        gladius::Vector2(2.f, 8.f),
        gladius::Vector2(2.f, 2.f)
    };
    // Second hole: square from (12,12) to (18,18)
    std::vector<gladius::Vector2> hole2Pts = {
        gladius::Vector2(12.f, 12.f),
        gladius::Vector2(18.f, 12.f),
        gladius::Vector2(18.f, 18.f),
        gladius::Vector2(12.f, 18.f),
        gladius::Vector2(12.f, 12.f)
    };
    
    gladius::PolyLine outer = createClosedPolyLine(outerPts);
    gladius::PolyLine hole1 = createClosedPolyLine(hole1Pts);
    gladius::PolyLine hole2 = createClosedPolyLine(hole2Pts);
    
    gladius::ContourExtractor extractor(makeDummyLogger());
    auto &contours = const_cast<std::vector<gladius::PolyLine>&>(extractor.getContour());
    contours.clear();
    contours.push_back(outer);
    contours.push_back(hole1);
    contours.push_back(hole2);
    
    // Run calcSign.
    extractor.calcAreas();
    extractor.calcSign();
    
    // Assert the expected signs for each contour. The order should be preserved.
    const auto &resultContours = extractor.getContour();
    EXPECT_EQ(resultContours.size(), 3);
    EXPECT_GT(resultContours[0].area, 0.f); // outer contour
    EXPECT_LT(resultContours[1].area, 0.f); // first hole
    EXPECT_LT(resultContours[2].area, 0.f); // second hole
}

// Test degenerate (nearly missing) contour: very few vertices or almost a line.
// Such cases are critical because missing edges might lead to undefined centroid behavior.
TEST(ContourExtractorTest, DegenerateContour)
{
    // Create a degenerate contour: three collinear points.
    std::vector<gladius::Vector2> degeneratePts = {
        gladius::Vector2(0.f, 0.f),
        gladius::Vector2(5.f, 5.f),
        gladius::Vector2(10.f, 10.f),
        gladius::Vector2(0.f, 0.f)
    };
    gladius::PolyLine degeneratePoly = createClosedPolyLine(degeneratePts);
    
    // Even if the computed area is zero (or near zero) the method should not crash.
    EXPECT_NEAR(degeneratePoly.area, 0.f, 1e-5f);
    
    gladius::ContourExtractor extractor(makeDummyLogger());
    auto &contours = const_cast<std::vector<gladius::PolyLine>&>(extractor.getContour());
    contours.clear();
    contours.push_back(degeneratePoly);
    
    // Running calcSign should not crash though the contour is degenerate.
    EXPECT_NO_THROW(extractor.calcSign());
    
    // Since it is degenerate, we expect the absolute area is zero.
    EXPECT_NEAR(extractor.getContour().front().area, 0.f, 1e-5f);
}


} // namespace gladius_tests