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

        /**
         * @brief Renders the function selection dropdown for a level set
         * 
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param levelSet The level set to modify
         * @param function The current function of the level set
         */
        static void renderFunctionDropdown(
            SharedDocument document, 
            Lib3MF::PModel model3mf, 
            Lib3MF::PLevelSet levelSet, 
            Lib3MF::PFunction function);

        /**
         * @brief Renders the channel selection dropdown for a level set
         * 
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param levelSet The level set to modify
         */
        static void renderChannelDropdown(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            Lib3MF::PLevelSet levelSet);

        /**
         * @brief Renders the mesh selection dropdown for a level set
         * 
         * @param document The document containing the meshes
         * @param model3mf The 3MF model containing the resources
         * @param levelSet The level set to modify
         */
        static void renderMeshDropdown(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            Lib3MF::PLevelSet levelSet);
    };
}
