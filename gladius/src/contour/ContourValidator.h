#pragma once

#include <optional>

#include "../Contour.h"

namespace gladius::contour
{
    struct ValidationResult
    {
        bool intersectionFree = false;
    };

    bool endCrossesStart(PolyLine & polyLine);

    ValidationResult validate(PolyLine & polyLine, size_t numberOfNeighbors = 50u);

}
