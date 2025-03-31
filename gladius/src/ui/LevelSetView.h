#pragma once

#include "Document.h"
#include <io/3mf/Writer3mf.h>
#include "imgui.h"
#include <lib3mf_implicit.hpp>

namespace gladius::ui
{
    class LevelSetView
    {
    public:
        /**
         * @brief Renders the level set properties in a table format
         * 
         * @param levelSet The level set to render
         * @param document The document containing the level set
         * @param model3mf The 3MF model containing the resources
         * 
         * @return true if the level set properties were modified
         */
        bool render(SharedDocument document) const;

        /**
         * @brief Renders the function selection dropdown for a level set
         * 
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param levelSet The level set to modify
         * @param function The current function of the level set
         * 
         * @return true if the function properties were modified
         */
        static bool renderFunctionDropdown(
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
         * 
         * @return true if the channel properties were modified
         */
        static bool renderChannelDropdown(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            Lib3MF::PLevelSet levelSet);

        /**
         * @brief Renders the mesh selection dropdown for a level set
         * 
         * @param document The document containing the meshes
         * @param model3mf The 3MF model containing the resources
         * @param levelSet The level set to modify
         * 
         * @return true if the mesh properties were modified
         */
        static bool renderMeshDropdown(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            Lib3MF::PLevelSet levelSet);
    };
}
