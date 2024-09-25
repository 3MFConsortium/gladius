#pragma once
#include "QuadTree.h"
#include "utils.h"


namespace gladius::contour
{
    
    float determineConnectionCost(PointWithNormal const & start, PointWithNormal const & end);

    PolyLines convertToPolylines(QuadTree & pointCloud, float maxVertexDistance);
}
