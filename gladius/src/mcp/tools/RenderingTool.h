/**
 * @file RenderingTool.h
 * @brief Header for RenderingTool - handles rendering operations for 3MF models
 */

#pragma once

#include "MCPToolBase.h"
#include <nlohmann/json.hpp>
#include <string>

namespace gladius
{
    namespace mcp::tools
    {
        /**
         * @brief Tool for handling rendering operations in Gladius
         *
         * This tool provides methods for:
         * - Rendering models to image files
         * - Rendering with custom camera settings
         * - Generating thumbnails
         * - Getting optimal camera positions
         * - Getting model bounding boxes
         */
        class RenderingTool : public MCPToolBase
        {
          public:
            /**
             * @brief Constructor
             * @param app Pointer to the Application instance
             */
            explicit RenderingTool(Application * app);

            /**
             * @brief Render the current model to an image file
             * @param outputPath Path where to save the rendered image
             * @param width Image width in pixels
             * @param height Image height in pixels
             * @param format Output format (png, jpg)
             * @param quality Quality setting for lossy formats (0.0-1.0)
             * @return JSON response with success status and any error messages
             */
            nlohmann::json renderToFile(const std::string & outputPath,
                                        unsigned int width = 1024,
                                        unsigned int height = 1024,
                                        const std::string & format = "png",
                                        float quality = 0.9f);

            /**
             * @brief Render the current model with custom camera settings
             * @param outputPath Path where to save the rendered image
             * @param cameraSettings JSON object with camera parameters
             * @param renderSettings JSON object with rendering parameters
             * @return JSON response with success status and any error messages
             */
            nlohmann::json renderWithCamera(const std::string & outputPath,
                                            const nlohmann::json & cameraSettings,
                                            const nlohmann::json & renderSettings);

            /**
             * @brief Generate a thumbnail image for the current model
             * @param outputPath Path where to save the thumbnail
             * @param size Thumbnail size in pixels (square)
             * @return JSON response with success status and any error messages
             */
            nlohmann::json generateThumbnail(const std::string & outputPath,
                                             unsigned int size = 256);

            /**
             * @brief Get the optimal camera position for viewing the current model
             * @return JSON response with camera position data
             */
            nlohmann::json getOptimalCameraPosition();

            /**
             * @brief Get the bounding box of the current model
             * @return JSON response with bounding box data
             */
            nlohmann::json getModelBoundingBox();
        };
    }
}
