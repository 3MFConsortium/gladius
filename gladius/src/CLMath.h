#pragma once

#include "gpgpu.h"

namespace gladius
{

    template <typename T>
    auto sign(T value) -> T
    {
        return static_cast<T>((T(0) < value) - (value < T(0)));
    }

    float lerp(float s, float e, float t);
    float blerp(float c00, float c10, float c01, float c11, float tx, float ty);

    float length(cl_float2 vector);

    auto distBetween(cl_float2 vector1, cl_float2 vector2) -> float;

    auto normalize(cl_float2 vector) -> cl_float2;

    cl_float2 normal(cl_float2 vector);

    cl_float2 normal(cl_float2 start, cl_float2 end);

    float angle(cl_float2 vectorA, cl_float2 vectorB);
}