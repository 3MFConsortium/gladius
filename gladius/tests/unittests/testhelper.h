#pragma once

#include <Eigen/Core>
#include <Eigen/src/Core/Matrix.h>

#include "nodes/Model.h"

#include <vector>

namespace gladius_tests
{
    using namespace gladius;
    using float3 = Eigen::Vector3f;
    using float2 = Eigen::Vector2f;
    using ShapeFunction = std::function<float(float3)>;
    namespace helper
    {
        template <typename T>
        auto countNumberOfNodesOfType(nodes::Model & model)
        {
            int count = 0;
            auto visitor = gladius::nodes::OnTypeVisitor<T>([&](T &) { ++count; });
            model.visitNodes(visitor);
            return count;
        }

        auto sphere(float3 pos, float radius) -> float;
        auto testModel(float3 pos) -> float;
        auto testModel2(float3 pos) -> float;

        template <typename Iterator>
        auto computeHash(Iterator cbegin, Iterator cend)
        {
            std::hash<typename std::iterator_traits<Iterator>::value_type> hasher;
            size_t hash = 0;
            for (auto it = cbegin; it != cend; ++it)
            {
                hash ^= hasher(*it) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    }
}
