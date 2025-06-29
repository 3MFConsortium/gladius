#pragma once
#include "imgui.h"
#include "nodesfwd.h"


namespace gladius::ui
{
    struct LinkColors
    {
        static constexpr ImVec4 ColorFloat = {0.6f, 0.6f, 1.f, 1.f};
        static constexpr ImVec4 ColorFloat3 = {0.6f, 1.f, 0.6f, 1.f};
        static constexpr ImVec4 ColorMatrix = {1.f, 0.6f, 0.6f, 1.f};
        static constexpr ImVec4 ColorResource = {1.f, 1.f, 0.6f, 1.f};
        static constexpr ImVec4 ColorString = {1.f, 0.6f, 1.f, 1.f};
        static constexpr ImVec4 ColorInt = {0.6f, 1.f, 1.f, 1.f};
        static constexpr ImVec4 ColorInvalid = {1.f, 0.f, 0.f, 1.f};

        static constexpr ImVec4 DarkColorFloat = {0.3f, 0.3f, 0.5f, 1.f};
        static constexpr ImVec4 DarkColorFloat3 = {0.3f, 0.5f, 0.3f, 1.f};
        static constexpr ImVec4 DarkColorMatrix = {0.5f, 0.3f, 0.3f, 1.f};
        static constexpr ImVec4 DarkColorResource = {0.5f, 0.5f, 0.3f, 1.f};
        static constexpr ImVec4 DarkColorString = {0.5f, 0.3f, 0.5f, 1.f};
        static constexpr ImVec4 DarkColorInt = {0.3f, 0.5f, 0.5f, 1.f};
        static constexpr ImVec4 DarkColorInvalid = {0.5f, 0.f, 0.f, 1.f};
    };;
} // namespace gladius::ui
