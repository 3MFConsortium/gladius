#include "utils.h"
#include <float.h>

namespace gladius::contour::utils
{
    std::optional<Vector2>
    intersectionOfTwoLineSegments(Vector2 p1, Vector2 p2, Vector2 p3, Vector2 p4, double tolerance)
    {
        // see https://de.wikipedia.org/w/index.php?title=Schnittpunkt&oldid=169771750
        const float determinant = (p1.x() * (p4.y() - p3.y()) + p2.x() * (p3.y() - p4.y()) +
                                   p4.x() * (p2.y() - p1.y()) + p3.x() * (p1.y() - p2.y()));

        if (fabs(determinant) < 0.f)
        {
            return {};
        }

        const float s =
          (p1.x() * (p4.y() - p3.y()) + p3.x() * (p1.y() - p4.y()) + p4.x() * (p3.y() - p1.y())) /
          determinant;
        const float t =
          -(p1.x() * (p3.y() - p2.y()) + p2.x() * (p1.y() - p3.y()) + p3.x() * (p2.y() - p1.y())) /
          determinant;

        if (((tolerance < s) && (s < 1.f - tolerance)) && (tolerance < t) && (t < 1.f - tolerance))
        {
            return p1 + s * (p2 - p1);
        }
        return {};
    }

    std::optional<Vector2>
    intersectionOfTwoLines(Vector2 const p1, Vector2 const p2, Vector2 const p3, Vector2 const p4)
    {
        Vector2 intersectionPoint;
        auto const determinant =
          ((p4.y() - p3.y()) * (p2.x() - p1.x()) - (p2.y() - p1.y()) * (p4.x() - p3.x()));

        if (fabs(determinant) <= FLT_EPSILON)
        {
            return {};
        }

        intersectionPoint.x() = (((p4.x() - p3.x()) * (p2.x() * p1.y() - p1.x() * p2.y()) -
                                  (p2.x() - p1.x()) * (p4.x() * p3.y() - p3.x() * p4.y())) /
                                 determinant);
        intersectionPoint.y() = (((p1.y() - p2.y()) * (p4.x() * p3.y() - p3.x() * p4.y()) -
                                  (p3.y() - p4.y()) * (p2.x() * p1.y() - p1.x() * p2.y())) /
                                 determinant);
        return intersectionPoint;
    }

}