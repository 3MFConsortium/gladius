

#pragma once
#include <string>

#include "Parameter.h"

struct ImVec4;

namespace gladius::ui
{
    void loadingIndicatorCircle(const char * label,
                                float radius,
                                const ImVec4 & main_color,
                                const ImVec4 & backdrop_color,
                                int numberOfDots,
                                float speed);

    bool angleEdit(const char * label, float * const angleInRadians);

    void hyperlink(std::string const & label, std::string const & url);

    void toggleButton(std::string const & label, bool * state);

    bool matrixEdit(std::string const & label, gladius::nodes::Matrix4x4 & matrix);

    bool floatEdit(std::string const & label, float & value);

    void frameOverlay(ImVec4 color);
}