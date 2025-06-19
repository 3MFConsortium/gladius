#pragma once
#include <eigen3/Eigen/Core>
#include <eigen3/Eigen/src/Core/Matrix.h>
#include <memory>

namespace gladius
{
    using Vector3 = Eigen::Vector3f;
    using ResourceId = uint32_t;

    // Forward declare classes
    class ResourceContext;
    class ComputeContext;
    class Primitives;

    // Define shared pointer types
    using SharedResources = std::shared_ptr<ResourceContext>;
    using SharedComputeContext = std::shared_ptr<ComputeContext>;
    using SharedPrimitives = std::shared_ptr<Primitives>;
}
