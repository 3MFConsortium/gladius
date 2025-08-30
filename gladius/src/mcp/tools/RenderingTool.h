/**
 * @file RenderingTool.h
 * @brief Header for RenderingTool - handles rendering operations for 3MF models
 */

#pragma once

#include "MCPToolBase.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

// Forward declarations
namespace gladius
{
    class ComputeCore;
}

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

          private:
            /**
             * @brief Get validated compute core from current document
             * @return Shared pointer to ComputeCore or nullptr if validation fails
             */
            std::shared_ptr<ComputeCore> getValidatedComputeCore();

            /**
             * @brief Prepare model for rendering by refreshing and validating
             * @param core The compute core to prepare
             * @return True if preparation successful, false otherwise
             */
            bool prepareModelForRendering(std::shared_ptr<ComputeCore> core);

            /**
             * @brief Validate render format
             * @param format The format string to validate
             * @return True if format is valid, false otherwise
             */
            bool validateRenderFormat(const std::string & format);

            /**
             * @brief Create output directory for the given path
             * @param outputPath The output file path
             */
            void createOutputDirectory(const std::string & outputPath);

            /**
             * @brief Create a standardized success response
             * @param message Success message
             * @param outputPath Output file path
             * @param requestedWidth Requested image width
             * @param requestedHeight Requested image height
             * @param format Image format
             * @param quality Image quality
             * @param additionalData Any additional data to include
             * @return JSON success response
             */
            nlohmann::json
            createSuccessResponse(const std::string & message,
                                  const std::string & outputPath,
                                  unsigned int requestedWidth,
                                  unsigned int requestedHeight,
                                  const std::string & format,
                                  float quality,
                                  const nlohmann::json & additionalData = nlohmann::json::object());
        };
    }
}
