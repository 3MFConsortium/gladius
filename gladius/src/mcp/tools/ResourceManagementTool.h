/**
 * @file ResourceManagementTool.h
 * @brief Tool for managing 3MF resources (level sets, volumetric data, cleanup)
 */

#pragma once

#include "MCPToolBase.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace gladius
{
    class Application; // Forward declaration

    namespace mcp::tools
    {
        /**
         * @brief Tool for managing 3MF resources including level sets, volumetric data, and cleanup
         *
         * This tool handles:
         * - Creating level sets from functions
         * - Creating volumetric color and property data
         * - Creating functions from 3D images
         * - Modifying level set references
         * - Removing unused resources
         */
        class ResourceManagementTool : public MCPToolBase
        {
          public:
            explicit ResourceManagementTool(Application * app);

            /**
             * @brief Create a level set from a function
             * @param functionId The ID of the function to convert to a level set
             * @return Pair of success flag and level set resource ID
             */
            std::pair<bool, uint32_t> createLevelSet(uint32_t functionId);

            /**
             * @brief Create a function from 3D image data
             * @param name Name for the new function
             * @param imagePath Path to the 3D image file
             * @param valueScale Scaling factor for image values
             * @param valueOffset Offset for image values
             * @return Pair of success flag and function resource ID
             */
            std::pair<bool, uint32_t> createImage3DFunction(const std::string & name,
                                                            const std::string & imagePath,
                                                            float valueScale = 1.0f,
                                                            float valueOffset = 0.0f);

            /**
             * @brief Create volumetric color data from a function
             * @param functionId The ID of the function to create color data from
             * @param channel The channel name to use for color data
             * @return Pair of success flag and color data resource ID
             */
            std::pair<bool, uint32_t> createVolumetricColor(uint32_t functionId,
                                                            const std::string & channel);

            /**
             * @brief Create volumetric property data from a function
             * @param propertyName Name of the custom property
             * @param functionId The ID of the function to create property data from
             * @param channel The channel name to use for property data
             * @return Pair of success flag and property data resource ID
             */
            std::pair<bool, uint32_t> createVolumetricProperty(const std::string & propertyName,
                                                               uint32_t functionId,
                                                               const std::string & channel);

            /**
             * @brief Modify an existing level set's function reference or channel
             * @param levelSetModelResourceId The ID of the level set to modify
             * @param functionModelResourceId Optional new function ID to reference
             * @param channel Optional new channel name
             * @return Success flag
             */
            bool modifyLevelSet(uint32_t levelSetModelResourceId,
                                std::optional<uint32_t> functionModelResourceId,
                                std::optional<std::string> channel);

            /**
             * @brief Remove all unused resources from the document
             * @return JSON response with success status and count of removed resources
             */
            nlohmann::json removeUnusedResources();
        };
    }
}
