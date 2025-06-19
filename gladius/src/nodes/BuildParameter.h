#pragma once
#include <filesystem>
#include <optional>

namespace gladius::nodes
{
    enum class ContourSorting
    {
        None,
        FromInsideToOutside
    };

    struct SliceParameter
    {
        ContourSorting sortingPolicy = ContourSorting::None;
        float simplifytolerance = 5E-1f;
        float zHeight_mm = 0.f;
        bool adoptGradientBased = true;
        float offset = 0.f;
    };
}