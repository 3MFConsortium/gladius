#pragma once

#include "types.h"
#include <string>

namespace gladius
{
    std::string toLowerCase(std::string text);

    nodes::Matrix4x4 zeroMatrix();
    nodes::Matrix4x4 identityMatrix();
    nodes::Matrix4x4 inverseMatrix(const nodes::Matrix4x4 & matrix);
}
