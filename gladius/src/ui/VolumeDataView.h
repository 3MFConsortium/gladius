#pragma once

#include "Document.h"
#include "imgui.h"
#include <io/3mf/Writer3mf.h>
#include <lib3mf_implicit.hpp>

namespace gladius::ui
{
    /**
     * @brief Class for displaying and editing VolumeData properties
     */
    class VolumeDataView
    {
      public:
        /**
         * @brief Renders the volume data properties in a table format
         *
         * @param document The document containing the volume data
         *
         * @return true if the volume data properties were modified
         */
        bool render(SharedDocument document) const;

        /**
         * @brief Checks if any color functions are available for use with volume data
         *
         * A color function is available if:
         * 1. It exists in the document's assembly
         * 2. It meets the criteria defined by isQualifiedForVolumeColor
         * 3. It has a corresponding resource in the 3MF model
         *
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         *
         * @return true if at least one qualified color function is available, false otherwise
         */
        static bool areColorFunctionsAvailable(SharedDocument const & document,
                                               Lib3MF::PModel const & model3mf);

        /**
         * @brief Renders the color function selection dropdown for volume data
         *
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param volumeData The volume data to modify
         * @param colorFunction The current color function of the volume data
         *
         * @return true if the function properties were modified
         */
        static bool renderColorFunctionDropdown(SharedDocument document,
                                                Lib3MF::PModel model3mf,
                                                Lib3MF::PVolumeData volumeData,
                                                Lib3MF::PVolumeDataColor colorFunction);

        /**
         * @brief Renders a dropdown for selecting a channel from a color function
         *
         * Displays a dropdown with all available float3 output channels from the selected function
         *
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param volumeData The volume data to modify
         * @param colorData The color data containing the channel information
         *
         * @return true if the channel selection was modified
         */
        static bool renderChannelDropdown(SharedDocument document,
                                          Lib3MF::PModel model3mf,
                                          Lib3MF::PVolumeData volumeData,
                                          Lib3MF::PVolumeDataColor colorData);

        /**
         * @brief Renders the property functions section for volume data
         *
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param volumeData The volume data to modify
         * @param propertyData The current property data of the volume data
         *
         * @return true if the property functions were modified
         */
        static bool renderPropertyFunctionsSection(SharedDocument document,
                                                   Lib3MF::PModel model3mf,
                                                   Lib3MF::PVolumeData volumeData,
                                                   Lib3MF::PVolumeDataProperty propertyData);

      private:
        /**
         * @brief Gets the name of a volume data resource
         *
         * @param volumeData The volume data to get the name for
         *
         * @return std::string The name of the volume data
         */
        static std::string getVolumeDataName(const Lib3MF::PVolumeData & volumeData);
    };
}
