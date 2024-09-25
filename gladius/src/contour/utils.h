#pragma once

#include "../Contour.h"

#include <optional>

namespace gladius::contour::utils
{
    std::optional<Vector2> intersectionOfTwoLineSegments(Vector2 p1,
                                                         Vector2 p2,
                                                         Vector2 p3,
                                                         Vector2 p4,
                                                         double tolerance = 5.E-2);

    std::optional<Vector2>
    intersectionOfTwoLines(Vector2 const p1, Vector2 const p2, Vector2 const p3, Vector2 const p4);
}
