
#include "testhelper.h"
#include <nodes/Model.h>

#include <Eigen/Core>
#include <Eigen/src/Core/Matrix.h>

namespace gladius_tests::helper
{
    auto sphere(float3 pos, float radius) -> float
    {
        return std::sqrt(pos.x() * pos.x() + pos.y() * pos.y() + pos.z() * pos.z()) - radius;
    }

    auto testModel(float3 pos) -> float
    {
        auto sdf = sphere(pos, 50.f);
        sdf = std::min(sdf, sphere(pos - float3{15.f, 15.f, 15.f}, 30.));
        return sdf;
    }

    auto testModel2(float3 pos) -> float
    {
        auto sdf = sphere(pos, 50.f);
        sdf = std::min(sdf, sphere(pos - float3{15.f, 15.f, 15.f}, 30.));
        return sdf;
    }
}