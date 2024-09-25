#include "ContourValidator.h"
#include "utils.h"

namespace gladius::contour
{
    std::optional<Vector2> hasIntersectionInRange(Vertices2d::const_iterator vertex,
                                                  Vertices2d::const_iterator begin,
                                                  Vertices2d::const_iterator end)
    {
        if (vertex == begin)
        {
            return {};
        }

        const auto prevVertex = *std::prev(vertex);

        for (auto iter = begin; std::next(iter) != end && iter != end; ++iter)
        {
            if (iter == vertex || iter == std::prev(vertex) || iter == std::next(vertex))
            {
                continue;
            }
            auto intersection = contour::utils::intersectionOfTwoLineSegments(
              *iter, *std::next(iter), prevVertex, *vertex);
            if (intersection.has_value())
            {
                return intersection;
            }
        }

        return {};
    }

    bool endCrossesStart(PolyLine & polyLine)
    {
        const auto lastVertexIter = std::prev(polyLine.vertices.cend());
        const auto intersection = hasIntersectionInRange(
          lastVertexIter, polyLine.vertices.cbegin(), polyLine.vertices.cend());
        if (intersection.has_value())
        {
            polyLine.selfIntersections.push_back(intersection.value());
            return true;
        }

        return false;
    }

    ValidationResult validate(PolyLine & polyLine, size_t numberOfNeighbors)
    {
        ValidationResult result;
        result.intersectionFree = true;
        if (polyLine.vertices.size() < 3)
        {
            return result;
        }
        polyLine.selfIntersections.clear();

        for (auto iter = std::next(polyLine.vertices.cbegin()); iter != polyLine.vertices.cend();
             ++iter)
        {
            const auto begin = std::prev(
              iter,
              std::min(static_cast<size_t>(std::distance(polyLine.vertices.cbegin(), iter)),
                       numberOfNeighbors));

            const auto end =
              std::next(iter,
                        std::min(static_cast<size_t>(std::distance(iter, polyLine.vertices.cend())),
                                 numberOfNeighbors));

            const auto intersection = hasIntersectionInRange(iter, begin, end);
            if (intersection.has_value())
            {
                result.intersectionFree = false;
                polyLine.selfIntersections.push_back(intersection.value());
            }
        }

        if (endCrossesStart(polyLine))
        {
            result.intersectionFree = false;
        }

        return result;
    }
}
