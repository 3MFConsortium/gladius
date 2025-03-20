#pragma once

#include "Document.h"
#include "imgui.h"

namespace gladius::ui
{
    class LevelSetView
    {
    public:
        void render(SharedDocument document) const;
    };
}
