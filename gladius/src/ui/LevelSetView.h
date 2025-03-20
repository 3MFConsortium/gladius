#pragma once

#include "Document.h"
#include "imgui.h"
#include <lib3mf/Cpp/lib3mf_implicit.hpp>

namespace gladius::ui
{
    class LevelSetView
    {
    public:
        void render(SharedDocument document) const;
        
    private:
        /**
         * @brief Renders the function selection dropdown for a level set
         * 
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param levelSet The level set to modify
         * @param function The current function of the level set
         */
        void renderFunctionDropdown(
            SharedDocument document, 
            Lib3MF::PModel model3mf, 
            Lib3MF::PLevelSet levelSet, 
            Lib3MF::PFunction function) const;

        /**
         * @brief Renders the channel selection dropdown for a level set
         * 
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param levelSet The level set to modify
         */

        void renderChannelDropdown(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            Lib3MF::PLevelSet levelSet) const;
    };
}
