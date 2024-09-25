
#include <cmath>
#include <gtest/gtest.h>

namespace gladius_tests
{
    struct float3
    {
        float x, y, z;

        float3 operator-(const float3 & other) const
        {
            float3 result;
            result.x = x - other.x;
            result.y = y - other.y;
            result.z = z - other.z;
            return result;
        }

        float3 operator*(const float3 & other) const
        {
            float3 result;
            result.x = x * other.x;
            result.y = y * other.y;
            result.z = z * other.z;
            return result;
        }
    };

    struct float4
    {
        float x, y, z, w;

        float4 operator+(const float4& other) const
        {
            float4 result;
            result.x = x + other.x;
            result.y = y + other.y;
            result.z = z + other.z;
            result.w = w + other.w;
            return result;
        }

        float4 operator*(float scalar) const
        {
            float4 result;
            result.x = x * scalar;
            result.y = y * scalar;
            result.z = z * scalar;
            result.w = w * scalar;
            return result;
        }
    };;

    struct int3
    {
        int x, y, z;

        int3 operator+(const int3 & other) const
        {
            int3 result;
            result.x = x + other.x;
            result.y = y + other.y;
            result.z = z + other.z;
            return result;
        }
    };
    ;

    namespace testee
    {
        float4 mix(float4 a, float4 b, float t)
        {
            float4 result;
            result.x = a.x * (1 - t) + b.x * t;
            result.y = a.y * (1 - t) + b.y * t;
            result.z = a.z * (1 - t) + b.z * t;
            result.w = a.w * (1 - t) + b.w * t;
            return result;
        }

        int3 convert_int3(float3 f)
        {
            int3 result;
            result.x = static_cast<int>(f.x);
            result.y = static_cast<int>(f.y);
            result.z = static_cast<int>(f.z);
            return result;
        }

        using Evaluator = std::function<float4(int3)>;

        float3 applyTilesStyle(float3 uvw, int3 tileStyle)
        {
            // Mock implementation, replace with actual logic
            return uvw;
        }

        float4 sampleImageLinear4f(float3 uvw,
                                   float3 dimensions,
                                   int start,
                                   int3 tileStyle,
                                   Evaluator getValue)
        {
            float4 color = {0.0f, 0.0f, 0.0f, 0.0f};
            if (start < 0)
            {
                return color;
            }
            float3 uvwMapped = applyTilesStyle(uvw, tileStyle);
            int3 dim = convert_int3(dimensions);
            float3 texelCoord = uvwMapped * dimensions;
            int3 coord = convert_int3(texelCoord);

            // trilinear interpolation
            int3 texelCoord000 = coord;
            int3 texelCoord100 = coord + int3{1, 0, 0};
            int3 texelCoord010 = coord + int3{0, 1, 0};
            int3 texelCoord110 = coord + int3{1, 1, 0};
            int3 texelCoord001 = coord + int3{0, 0, 1};
            int3 texelCoord101 = coord + int3{1, 0, 1};
            int3 texelCoord011 = coord + int3{0, 1, 1};
            int3 texelCoord111 = coord + int3{1, 1, 1};

            float3 relPos =
              texelCoord -
              float3{std::floor(texelCoord.x), std::floor(texelCoord.y), std::floor(texelCoord.z)};
            float4 c000 = getValue(texelCoord000);
            float4 c100 = getValue(texelCoord100);
            float4 c010 = getValue(texelCoord010);
            float4 c110 = getValue(texelCoord110);
            float4 c001 = getValue(texelCoord001);
            float4 c101 = getValue(texelCoord101);
            float4 c011 = getValue(texelCoord011);
            float4 c111 = getValue(texelCoord111);
            float4 c00 = mix(c000, c100, relPos.x);
            float4 c01 = mix(c001, c101, relPos.x);
            float4 c10 = mix(c010, c110, relPos.x);
            float4 c11 = mix(c011, c111, relPos.x);

            float4 c0 = mix(c00, c10, relPos.y);
            float4 c1 = mix(c01, c11, relPos.y);
            color = mix(c0, c1, relPos.z);
            return color;
        }
    }

    // Define the test cases
    TEST(SampleImageLinear4f, EvaluationAtGridPoints_IntepolationHasNoEffect)
    {
        // Arrange
        auto const dimensions = float3{4.0f, 4.0f, 4.0f};
        auto const tileStyle = int3{1, 1, 1};
        auto const start = 0;
        auto const getValue = [](int3 coord) -> float4 {
            return {static_cast<float>(coord.x), static_cast<float>(coord.y), static_cast<float>(coord.z), 1.0f};
        };

        // Act
        for (int x = 0; x < 4; x++)
        {
            for (int y = 0; y < 4; y++)
            {
                for (int z = 0; z < 4; z++)
                {
                    auto const uvw = float3{static_cast<float>(x) / 4.0f, static_cast<float>(y) / 4.0f,
                                             static_cast<float>(z) / 4.0f};
                    auto const result = testee::sampleImageLinear4f(uvw, dimensions, start, tileStyle, getValue);
                    auto const expected = getValue(int3{x, y, z});
                    EXPECT_FLOAT_EQ(result.x, expected.x);
                    EXPECT_FLOAT_EQ(result.y, expected.y);
                    EXPECT_FLOAT_EQ(result.z, expected.z);
                    EXPECT_FLOAT_EQ(result.w, expected.w);
                }
            }
        }
    }


    TEST(SampleImageLinear4f, EvaluationHalfBetweenGridPointsInX_AveragOfLeftAndRight)
    {
        // Arrange
        auto const dimensions = float3{4.0f, 4.0f, 4.0f};
        auto const tileStyle = int3{1, 1, 1};
        auto const start = 0;
        auto const getValue = [](int3 coord) -> float4 {
            return {static_cast<float>(coord.x), 0.f, 0.f, 0.f};
        };

        // Act
        for (int y = 0; y < 4; y++)
        {
            for (int z = 0; z < 4; z++)
            {
                auto const uvw = float3{1.5f / 4.f, static_cast<float>(y) / 4.f, static_cast<float>(z) / 4.f};
                auto const result = testee::sampleImageLinear4f(uvw, dimensions, start, tileStyle, getValue);
                auto const expectedLeft = getValue(int3{1, y, z});
                auto const expectedRight = getValue(int3{2, y, z});
                auto const expectedAverage = (expectedLeft + expectedRight) * 0.5f;
                EXPECT_FLOAT_EQ(result.x, expectedAverage.x);
            }
        }
    }
}