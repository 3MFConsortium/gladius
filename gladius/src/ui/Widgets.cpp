#include <algorithm>

#include "Widgets.h"

#include "Parameter.h"

#ifdef WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

#include <filesystem>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_internal.h>
#define _USE_MATH_DEFINES
#include <array>
#include <imgui_stdlib.h>
#include <math.h>

namespace gladius::ui
{

    void loadingIndicatorCircle(const char * label,
                                const float radius,
                                const ImVec4 & main_color,
                                const ImVec4 & backdropColor,
                                const int numberOfDots,
                                const float speed)
    {
        ImGuiWindow * window = ImGui::GetCurrentWindow();
        if (window->SkipItems)
        {
            return;
        }

        const ImGuiID id = window->GetID(label);

        const ImVec2 pos = window->DC.CursorPos;
        const float radiusCircle = radius / 10.0f;
        const ImRect bb(pos, ImVec2(pos.x + radius * 2.5f, pos.y + radius * 2.5f));

        ImGui::ItemSize(bb, ImGui::GetStyle().FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
        {
            return;
        }
        auto const time = static_cast<float>(ImGui::GetTime());
        auto const angleIncrement = 2.0f * static_cast<float>(M_PI) / numberOfDots;
        for (int i = 0; i < numberOfDots; ++i)
        {
            auto const angle = angleIncrement * i;
            auto const x = radius * sinf(angle);
            auto const y = radius * cosf(angle) - radius * 0.3f;
            auto const growth = std::max(0.f, sin(time * speed - angle));
            ImVec4 color{
              main_color.x * growth + backdropColor.x * (1.0f - growth),
              main_color.y * growth + backdropColor.y * (1.0f - growth),
              main_color.z * growth + backdropColor.z * (1.0f - growth),
              main_color.w * growth + backdropColor.w * (1.0f - growth),
            };

            window->DrawList->AddCircleFilled(ImVec2(pos.x + radius + x, pos.y + radius - y),
                                              radiusCircle + growth * radiusCircle,
                                              ImGui::GetColorU32(color));
        }
        ImGui::TextUnformatted(label);
    }

    bool angleEdit(const char * label, float * const angleInRadians)
    {
        auto constexpr increment = 1.f;
        bool changed = false;
        static float const pi = static_cast<float>(M_PI);
        float angleInDegree = *angleInRadians * 180.f / pi;

        ImGui::SetNextItemWidth(200.f);
        changed |= ImGui::DragFloat(
          label,
          &angleInDegree,
          increment,
          -std::numeric_limits<float>::max(),
          std::numeric_limits<float>::max(),
          fmt::format("{:.3f} ° ({:.3f} rad)", angleInDegree, *angleInRadians).c_str());

        *angleInRadians = angleInDegree * pi / 180.f;

        if (ImGui::Button("45°"))
        {
            *angleInRadians = static_cast<float>(M_PI_4);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("90°"))
        {
            *angleInRadians = static_cast<float>(M_PI_2);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("180°"))
        {
            *angleInRadians = static_cast<float>(M_PI);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("270°"))
        {
            *angleInRadians = static_cast<float>(M_PI + M_PI_2);
            changed = true;
        }

        return changed;
    }

    void hyperlink(std::string const & label, std::string const & url)
    {
        ImGui::TextUnformatted(label.c_str());
        if (ImGui::IsItemClicked())
        {
#ifdef WIN32
            ShellExecute(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOW);
#endif
        }
    }

    void toggleButton(std::string const & label, bool * state)
    {
        if (*state)
        {
            if (ImGui::Button(label.c_str()))
            {
                *state = false;
            }
        }
        else
        {
            if (ImGui::MenuItem(label.c_str()))
            {
                *state = true;
            }
        }
    }

    bool matrixEdit(std::string const & label, gladius::nodes::Matrix4x4 & matrix)
    {
        ImGui::TextUnformatted(label.c_str());
        nodes::Matrix4x4 matrixCopy = matrix;

        bool changed = false;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {

                changed |= ImGui::InputFloat(fmt::format("##{}_{}_{}", label, row, col).c_str(),
                                             &matrixCopy[col][row],
                                             1.E-3f,
                                             1.E+2f,
                                             "%.8f");
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }

        if (changed)
        {
            matrix = matrixCopy;
        }
        return changed;
    }

    bool floatEdit(std::string const & label, float & value)
    {

        int digitCount = (value != 0.0f) ? (int) log10(fabs(value)) + 3 : 3;
        float const increment = std::max(powf(10, round(log10(fabs(value)))) * 0.01f, 0.1f);
        std::string format = "%." + std::to_string(digitCount) + "f";
        bool changed = ImGui::DragFloat(label.c_str(),
                                        &value,
                                        0.1f,
                                        -std::numeric_limits<float>::max(),
                                        std::numeric_limits<float>::max(),
                                        format.c_str());

        float const deltaTime = ImGui::GetIO().DeltaTime;

        if (ImGui::IsItemFocused())
        {
            int const keyPressCountUp =
              ImGui::GetKeyPressedAmount(ImGui::GetKeyIndex(ImGuiKey_UpArrow), deltaTime, 0.1f);
            if (keyPressCountUp > 0)
            {
                value += increment * keyPressCountUp;
                changed = true;
            }

            int const keyPressCountDown =
              ImGui::GetKeyPressedAmount(ImGui::GetKeyIndex(ImGuiKey_DownArrow), deltaTime, 0.1f);
            if (keyPressCountDown > 0)
            {
                value -= increment * keyPressCountDown;
                changed = true;
            }
        }

        return changed;
    }

    void frameOverlay(ImVec4 color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f))
    {
        ImVec2 rectMin = ImGui::GetItemRectMin();
        ImVec2 rectMax = ImGui::GetItemRectMax();
        rectMin.x += ImGui::GetStyle().FramePadding.x;
        rectMax.x =
          ImGui::GetContentRegionMax().x; // Expand to the right to the available content area

        ImGui::GetWindowDrawList()->AddRectFilled(rectMin,
                                                  rectMax,
                                                  ImGui::ColorConvertFloat4ToU32(color),
                                                  15.0f); // Gray color with rounded corners
    }
}
