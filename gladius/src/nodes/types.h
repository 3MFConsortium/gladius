#pragma once

#include <array>
#include <cstdint>

namespace gladius::nodes
{
    struct float3
    {
        float3() = default;
        float3(float x, float y, float z)
            : x(x)
            , y(y)
            , z(z)
        {
        }

        float x = 0.f;
        float y = 0.f;
        float z = 0.f;
    };

    struct float2
    {
        float2() = default;
        float x = 0.f;
        float y = 0.f;
    };

    using Matrix4x4 = std::array<std::array<float, 4>, 4>;

    enum class ContentType
    {
        Generic,
        Color,
        Length,
        Amount,
        Angle,
        Text,
        Index,
        Part,
        Transformation
    };

    using ResourceId = uint32_t;

}