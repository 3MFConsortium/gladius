#pragma once

#include "Document.h"
#include <io/3mf/Writer3mf.h>
#include "imgui.h"
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
         * @brief Renders the color function selection dropdown for volume data
         * 
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param volumeData The volume data to modify
         * @param colorFunction The current color function of the volume data
         * 
         * @return true if the function properties were modified
         */
        static bool renderColorFunctionDropdown(
            SharedDocument document, 
            Lib3MF::PModel model3mf, 
            Lib3MF::PVolumeData volumeData, 
            Lib3MF::PVolumeDataColor colorFunction);
            
        /**
         * @brief Renders the composite materials section for volume data
         * 
         * @param document The document containing the functions
         * @param model3mf The 3MF model containing the resources
         * @param volumeData The volume data to modify
         * @param compositeData The current composite data of the volume data
         * 
         * @return true if the composite properties were modified
         */
        static bool renderCompositeMaterialsSection(
            SharedDocument document,
            Lib3MF::PModel model3mf,
            Lib3MF::PVolumeData volumeData,
            Lib3MF::PVolumeDataComposite compositeData);
            
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
        static bool renderPropertyFunctionsSection(
            SharedDocument document,
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
        static std::string getVolumeDataName(const Lib3MF::PVolumeData& volumeData);
        

    };
}
