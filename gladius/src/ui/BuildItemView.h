#pragma once

#include "Document.h"
#include <lib3mf_implicit.hpp>

namespace gladius::ui
{
    /**
     * @brief UI component for viewing and managing build items
     */
    class BuildItemView
    {
      public:
        /**
         * @brief Default constructor
         */
        BuildItemView() = default;

        /**
         * @brief Renders the build item view
         *
         * @param document The shared document containing build items
         * @return true If any properties were changed
         * @return false If no properties were changed
         */
        [[nodiscard]] bool render(SharedDocument document) const;

        /**
         * @brief Renders the object resource dropdown for a build item
         *
         * @param document The document context
         * @param model3mf The 3MF model
         * @param buildItem The build item to modify
         * @return true If the object selection was changed
         * @return false If no changes were made
         */
        [[nodiscard]] static bool renderObjectDropdown(SharedDocument document,
                                                       Lib3MF::PModel model3mf,
                                                       Lib3MF::PBuildItem buildItem);

        /**
         * @brief Renders transformation controls for a build item
         *
         * @param document The document context
         * @param model3mf The 3MF model
         * @param buildItem The build item to modify
         * @return true If the transformation was changed
         * @return false If no changes were made
         */
        [[nodiscard]] static bool renderTransformControls(SharedDocument document,
                                                          Lib3MF::PModel model3mf,
                                                          Lib3MF::PBuildItem buildItem);
    };
} // namespace gladius::ui