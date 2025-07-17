#include "CLMath.h"

#include <cmath>
#include <limits>

namespace gladius
{
    float lerp(float s, float e, float t)
    {
        return s + (e - s) * t;
    }

    float blerp(float c00, float c10, float c01, float c11, float tx, float ty)
    {
        return lerp(lerp(c00, c10, tx), lerp(c01, c11, tx), ty);
    }

    float length(cl_float2 vector)
    {
        return std::sqrt(vector.x * vector.x + vector.y * vector.y);
    }

    float distBetween(cl_float2 vector1, cl_float2 vector2)
    {
        return length({{vector1.x - vector2.x, vector1.y - vector2.y}});
    }

    auto normalize(cl_float2 vector) -> cl_float2
    {
        auto const magnitude = length(vector);
        if (magnitude < std::numeric_limits<float>::epsilon())
        {
            return vector;
        }
        return cl_float2{vector.x / magnitude, vector.y / magnitude};
    }

    cl_float2 normal(cl_float2 vector)
    {
        vector = normalize(vector);
        return {vector.y, -vector.x};
    }

    cl_float2 normal(cl_float2 start, cl_float2 end)
    {
        cl_float2 const vector{end.x - start.x, end.y - start.y};
        return normal(vector);
    }

    float angle(cl_float2 vectorA, cl_float2 vectorB)
    {
        auto const dot = vectorA.x * vectorB.x + vectorA.y * vectorB.y;
        auto const det = vectorA.x * vectorB.y - vectorA.y * vectorB.x;
        return atan2(det, dot);
    }
}