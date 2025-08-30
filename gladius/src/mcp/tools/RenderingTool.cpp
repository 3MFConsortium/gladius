/**
 * @file RenderingTool.cpp
 * @brief Implementation of RenderingTool for rendering operations
 */

#include "RenderingTool.h"
#include "../../Application.h"
#include "../../Document.h"
#include "../../ui/OrbitalCamera.h"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace gladius
{
    namespace mcp::tools
    {
        RenderingTool::RenderingTool(Application * app)
            : MCPToolBase(app)
        {
        }

        std::shared_ptr<ComputeCore> RenderingTool::getValidatedComputeCore()
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                setErrorMessage("No active document");
                return nullptr;
            }

            auto core = document->getCore();
            if (!core)
            {
                setErrorMessage("No compute core available");
                return nullptr;
            }

            return core;
        }

        bool RenderingTool::prepareModelForRendering(std::shared_ptr<ComputeCore> core)
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                setErrorMessage("No active document");
                return false;
            }

            // Ensure assembly is updated before rendering
            try
            {
                document->refreshModelBlocking();
            }
            catch (const std::exception & e)
            {
                setErrorMessage("Assembly update failed: " + std::string(e.what()));
                return false;
            }

            // Prepare the model for rendering
            if (!core->prepareImageRendering())
            {
                setErrorMessage(
                  "Model preparation for rendering failed: This may be due to model "
                  "compilation errors, SDF precomputation failure, or invalid bounding box. "
                  "Check model validation for detailed OpenCL compilation errors.");
                return false;
            }

            return true;
        }

        bool RenderingTool::validateRenderFormat(const std::string & format)
        {
            if (format != "png" && format != "jpg")
            {
                setErrorMessage("Unsupported format: " + format + ". Supported formats: png, jpg");
                return false;
            }
            return true;
        }

        void RenderingTool::createOutputDirectory(const std::string & outputPath)
        {
            std::filesystem::path outputFilePath(outputPath);
            std::filesystem::create_directories(outputFilePath.parent_path());
        }

        nlohmann::json RenderingTool::createSuccessResponse(const std::string & message,
                                                            const std::string & outputPath,
                                                            unsigned int requestedWidth,
                                                            unsigned int requestedHeight,
                                                            const std::string & format,
                                                            float quality,
                                                            const nlohmann::json & additionalData)
        {
            nlohmann::json response;
            response["success"] = true;
            response["message"] = message;
            response["outputPath"] = outputPath;
            response["actualWidth"] = 256;  // Current thumbnail system limitation
            response["actualHeight"] = 256; // Current thumbnail system limitation
            response["requestedWidth"] = requestedWidth;
            response["requestedHeight"] = requestedHeight;
            response["format"] = format;
            response["quality"] = quality;

            // Merge any additional data
            for (auto & [key, value] : additionalData.items())
            {
                response[key] = value;
            }

            return response;
        }

        nlohmann::json RenderingTool::renderToFile(const std::string & outputPath,
                                                   unsigned int width,
                                                   unsigned int height,
                                                   const std::string & format,
                                                   float quality)
        {
            nlohmann::json response;

            if (!validateActiveDocument())
            {
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            // Validate format
            if (!validateRenderFormat(format))
            {
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            try
            {
                // Get validated compute core
                auto core = getValidatedComputeCore();
                if (!core)
                {
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                // Prepare model for rendering
                if (!prepareModelForRendering(core))
                {
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                // Create output directory and render
                createOutputDirectory(outputPath);
                core->saveThumbnail(outputPath);

                return createSuccessResponse(
                  "Rendering completed successfully (using thumbnail system - "
                  "currently limited to 256x256 resolution)",
                  outputPath,
                  width,
                  height,
                  format,
                  quality);
            }
            catch (const std::exception & e)
            {
                setErrorMessage("Rendering failed: " + std::string(e.what()));
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }
        }

        nlohmann::json RenderingTool::renderWithCamera(const std::string & outputPath,
                                                       const nlohmann::json & cameraSettings,
                                                       const nlohmann::json & renderSettings)
        {
            nlohmann::json response;

            if (!validateActiveDocument())
            {
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            // Parse camera settings
            if (!cameraSettings.contains("eye_position") ||
                !cameraSettings.contains("target_position"))
            {
                setErrorMessage(
                  "Camera settings must contain 'eye_position' and 'target_position'");
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            auto eyePos = cameraSettings["eye_position"];
            auto targetPos = cameraSettings["target_position"];

            if (!eyePos.is_array() || eyePos.size() != 3 || !targetPos.is_array() ||
                targetPos.size() != 3)
            {
                setErrorMessage("Camera positions must be arrays of 3 numbers [x, y, z]");
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            // Extract render settings (with defaults)
            std::string format = renderSettings.value("format", "png");
            unsigned int width = renderSettings.value("width", 256);
            unsigned int height = renderSettings.value("height", 256);
            float quality = renderSettings.value("quality", 0.9f);

            // Validate format
            if (!validateRenderFormat(format))
            {
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            try
            {
                // Get validated compute core
                auto core = getValidatedComputeCore();
                if (!core)
                {
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                // Prepare model for rendering
                if (!prepareModelForRendering(core))
                {
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                // Store current camera settings to restore later
                auto originalEyePos = core->getResourceContext()->getEyePosition();
                auto originalViewMat = core->getResourceContext()->getModelViewPerspectiveMat();

                try
                {
                    // Create and configure orbital camera with provided settings
                    ui::OrbitalCamera customCamera;

                    // Set the look-at target
                    ui::Position lookAtPos(targetPos[0].get<float>(),
                                           targetPos[1].get<float>(),
                                           targetPos[2].get<float>());
                    customCamera.setLookAt(lookAtPos);

                    // Calculate angles from eye position and target
                    ui::Position eyePosition(
                      eyePos[0].get<float>(), eyePos[1].get<float>(), eyePos[2].get<float>());

                    // Calculate direction vector from target to eye
                    ui::Position direction = eyePosition - lookAtPos;

                    // Calculate pitch and yaw from direction vector
                    float distance = direction.norm();
                    float pitch = std::asin(direction.z() / distance);
                    float yaw = std::atan2(direction.y(), direction.x());

                    customCamera.setAngle(pitch, yaw);
                    customCamera.update(10000.0f);

                    // Apply the custom camera settings
                    core->applyCamera(customCamera);

                    // Create output directory and render
                    createOutputDirectory(outputPath);
                    core->saveThumbnail(outputPath);

                    // Restore original camera settings
                    core->getResourceContext()->setEyePosition(originalEyePos);
                    core->getResourceContext()->setModelViewPerspectiveMat(originalViewMat);

                    nlohmann::json additionalData = {{"cameraSettings", cameraSettings},
                                                     {"renderSettings", renderSettings}};

                    return createSuccessResponse(
                      "Camera-based rendering completed successfully (using thumbnail system - "
                      "currently limited to 256x256 resolution)",
                      outputPath,
                      width,
                      height,
                      format,
                      quality,
                      additionalData);
                }
                catch (const std::exception & e)
                {
                    // Restore original camera settings on error
                    core->getResourceContext()->setEyePosition(originalEyePos);
                    core->getResourceContext()->setModelViewPerspectiveMat(originalViewMat);
                    throw; // Re-throw the exception
                }
            }
            catch (const std::exception & e)
            {
                setErrorMessage("Camera-based rendering failed: " + std::string(e.what()));
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }
        }

        nlohmann::json RenderingTool::generateThumbnail(const std::string & outputPath,
                                                        unsigned int size)
        {
            nlohmann::json response;

            if (!validateActiveDocument())
            {
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            try
            {
                // Get validated compute core
                auto core = getValidatedComputeCore();
                if (!core)
                {
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                // Prepare model for rendering
                if (!prepareModelForRendering(core))
                {
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                // Create output directory and generate thumbnail
                createOutputDirectory(outputPath);
                core->saveThumbnail(outputPath);

                nlohmann::json additionalData = {{"size", size}};
                return createSuccessResponse("Thumbnail generated successfully",
                                             outputPath,
                                             size,
                                             size,
                                             "png",
                                             1.0f,
                                             additionalData);
            }
            catch (const std::exception & e)
            {
                setErrorMessage("Thumbnail generation failed: " + std::string(e.what()));
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }
        }

        nlohmann::json RenderingTool::getOptimalCameraPosition()
        {
            nlohmann::json response;

            if (!validateActiveDocument())
            {
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            try
            {
                // Get validated compute core
                auto core = getValidatedComputeCore();
                if (!core)
                {
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                // Get the bounding box to calculate optimal camera position
                auto boundingBoxOpt = core->getBoundingBox();
                if (!boundingBoxOpt.has_value())
                {
                    setErrorMessage("No bounding box available for camera position calculation");
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                auto bbox = boundingBoxOpt.value();

                // Create an orbital camera and set it up optimally for the bounding box
                ui::OrbitalCamera optimalCamera;

                // Set good viewing angles (similar to thumbnail generation)
                optimalCamera.setAngle(0.6f, -2.0f);

                // Center the camera on the bounding box
                optimalCamera.centerView(bbox);

                // Update camera position
                optimalCamera.update(10000.0f);

                // Adjust distance to fit the model in view (using reasonable viewport size)
                const float viewportSize = 512.0f;
                optimalCamera.adjustDistanceToTarget(bbox, viewportSize, viewportSize);

                // Final update
                optimalCamera.update(10000.0f);

                // Get the calculated camera parameters
                auto eyePosition = optimalCamera.getEyePosition();
                auto lookAt = optimalCamera.getLookAt();

                response["success"] = true;
                response["message"] = "Optimal camera position calculated successfully";
                response["camera_settings"] = {
                  {"eye_position", {eyePosition.x, eyePosition.y, eyePosition.z}},
                  {"target_position", {lookAt.x, lookAt.y, lookAt.z}},
                  {"up_vector", {0.0f, 0.0f, 1.0f}}, // Standard up vector
                  {"field_of_view", 45.0f}           // Standard FOV
                };
                return response;
            }
            catch (const std::exception & e)
            {
                setErrorMessage("Camera position calculation failed: " + std::string(e.what()));
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }
        }

        nlohmann::json RenderingTool::getModelBoundingBox()
        {
            nlohmann::json response;

            if (!validateActiveDocument())
            {
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }

            try
            {
                // Get validated compute core
                auto core = getValidatedComputeCore();
                if (!core)
                {
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                // Get the bounding box from the compute core
                auto boundingBoxOpt = core->getBoundingBox();
                if (!boundingBoxOpt.has_value())
                {
                    setErrorMessage(
                      "No bounding box available - model may not be compiled or valid");
                    response["success"] = false;
                    response["error"] = getLastErrorMessage();
                    return response;
                }

                auto bbox = boundingBoxOpt.value();

                // Calculate center and size
                std::array<float, 3> min = {bbox.min.x, bbox.min.y, bbox.min.z};
                std::array<float, 3> max = {bbox.max.x, bbox.max.y, bbox.max.z};
                std::array<float, 3> center = {(bbox.min.x + bbox.max.x) / 2.0f,
                                               (bbox.min.y + bbox.max.y) / 2.0f,
                                               (bbox.min.z + bbox.max.z) / 2.0f};
                std::array<float, 3> size = {
                  bbox.max.x - bbox.min.x, bbox.max.y - bbox.min.y, bbox.max.z - bbox.min.z};

                response["success"] = true;
                response["message"] = "Bounding box calculated successfully";
                response["boundingBox"] = {
                  {"min", min}, {"max", max}, {"center", center}, {"size", size}};
                return response;
            }
            catch (const std::exception & e)
            {
                setErrorMessage("Bounding box calculation failed: " + std::string(e.what()));
                response["success"] = false;
                response["error"] = getLastErrorMessage();
                return response;
            }
        }
    }
}
