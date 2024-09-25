#pragma once

#include <eigen3/Eigen/Core>

#include <memory>
#include <vector>

namespace gladius
{
    using Vector2 = Eigen::Vector2f;

    enum class ContourMode
    {
        Inner = 0,
        Outer = 1,
        OpenLine = 2,
        ExcludeFromSlice
    };
    using ShellIndex = int;

    using Vertices2d = std::vector<Vector2>;

    struct PolyLine
    {
        ContourMode contourMode = ContourMode::ExcludeFromSlice;
        Vertices2d vertices;
        bool isClosed = false;
        bool hasIntersections = false;
        float area = 0.f;
        Vertices2d selfIntersections;
    };

    using PolyLines = std::vector<PolyLine>;
    using SharedPolyLines = std::shared_ptr<gladius::PolyLines>;
}
