#pragma once

#include "Document.h"
#include <lib3mf_implicit.hpp>

namespace gladius::ui
{
    /**
     * @brief UI component for viewing and managing 3MF model metadata
     */
    class MetaDataView
    {
      public:
        /**
         * @brief Default constructor
         */
        MetaDataView() = default;

        /**
         * @brief Renders the metadata view
         *
         * @param document The shared document containing the 3MF model with metadata
         * @return true If any properties were changed
         * @return false If no properties were changed
         */
        [[nodiscard]] bool render(SharedDocument document) const;

        /**
         * @brief Renders a table showing all metadata entries
         *
         * @param document The document context
         * @param model3mf The 3MF model
         * @return true If any metadata was changed
         * @return false If no changes were made
         */
        [[nodiscard]] static bool renderMetaDataTable(SharedDocument document,
                                                      Lib3MF::PModel model3mf);

        /**
         * @brief Adds a new metadata entry
         *
         * @param document The document context
         * @param model3mf The 3MF model
         * @return true If a new entry was added
         * @return false If no entry was added
         */
        [[nodiscard]] static bool renderAddMetaDataEntry(SharedDocument document,
                                                         Lib3MF::PModel model3mf);
    };
} // namespace gladius::ui
