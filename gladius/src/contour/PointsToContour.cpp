#include "PointsToContour.h"

#include "utils.h"

namespace gladius::contour
{
    float determineConnectionCost(PointWithNormal const & start, PointWithNormal const & end)
    {
        auto const startTangentDirection = Vector2{start.normal.y(), -start.normal.x()};
        auto const endTangentDirection = Vector2{end.normal.y(), -end.normal.x()};

        auto const tangentIntersection = utils::intersectionOfTwoLineSegments(
          start.position, startTangentDirection, end.position, endTangentDirection);

        if (!tangentIntersection.has_value())
        {
            return (end.position - start.position).squaredNorm();
        }

        return (tangentIntersection.value() - start.position).squaredNorm() +
               (tangentIntersection.value() - end.position).squaredNorm();
    }

    PolyLines convertToPolylines(QuadTree & pointCloud, float maxVertexDistance)
    {

        auto start = pointCloud.getAnyPoint();
        if (!start.has_value())
        {
            return {};
        }

        PolyLines polyLines;
        while (start.has_value())
        {
            PolyLine poly;

            poly.vertices.push_back(start->position);

            auto currentVertex = start.value();

            pointCloud.remove(currentVertex);
            while (true)
            { // find best neighbor
                auto neighbors =
                  pointCloud.findNeighbors(currentVertex.position, maxVertexDistance);

                if (neighbors.empty())
                {
                    break;
                }
                auto bestOption =
                  std::min_element(neighbors.cbegin(),
                                   neighbors.cend(),
                                   [&currentVertex](auto const & a, auto const & b)
                                   {
                                       return determineConnectionCost(currentVertex, a) <
                                              determineConnectionCost(currentVertex, b);
                                   });

                poly.vertices.push_back(bestOption->position);
                currentVertex = *bestOption;
                pointCloud.remove(*bestOption);
            }

            poly.isClosed = true;
            poly.contourMode = ContourMode::OpenLine;
            polyLines.push_back(std::move(poly));

            pointCloud.remove(start.value());
            start = pointCloud.getAnyPoint();
        }

        return polyLines;
    }
}
