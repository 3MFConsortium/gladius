/**
 * @file MCPApplicationInterface.h
 * @brief Minimal interface for MCP server to interact with Application
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "FunctionArgument.h"

namespace gladius
{
    /**
     * @brief Minimal interface for MCP server to access Application functionality
     * This avoids pulling in heavy Application.h dependencies that cause build issues
     */
    class MCPApplicationInterface
    {
      public:
        virtual ~MCPApplicationInterface() = default;

        // Basic application info
        virtual std::string getVersion() const = 0;
        virtual bool isRunning() const = 0;
        virtual std::string getApplicationName() const = 0;

        // Status information
        virtual std::string getStatus() const = 0;

        // UI / Headless control
        virtual void setHeadlessMode(bool headless) = 0;
        virtual bool isHeadlessMode() const = 0;
        virtual bool showUI() = 0;
        virtual bool isUIRunning() const = 0;

        // Document operations
        virtual bool hasActiveDocument() const = 0;
        virtual std::string getActiveDocumentPath() const = 0;

        // Document lifecycle operations
        virtual bool createNewDocument() = 0;
        virtual bool openDocument(const std::string & path) = 0;
        virtual bool saveDocument() = 0;
        virtual bool saveDocumentAs(const std::string & path) = 0;
        virtual bool exportDocument(const std::string & path, const std::string & format) = 0;

        // Parameter operations
        virtual bool setFloatParameter(uint32_t modelId,
                                       const std::string & nodeName,
                                       const std::string & parameterName,
                                       float value) = 0;
        virtual float getFloatParameter(uint32_t modelId,
                                        const std::string & nodeName,
                                        const std::string & parameterName) = 0;
        virtual bool setStringParameter(uint32_t modelId,
                                        const std::string & nodeName,
                                        const std::string & parameterName,
                                        const std::string & value) = 0;
        virtual std::string getStringParameter(uint32_t modelId,
                                               const std::string & nodeName,
                                               const std::string & parameterName) = 0;

        // Expression and function operations
        virtual std::pair<bool, uint32_t>
        createFunctionFromExpression(const std::string & name,
                                     const std::string & expression,
                                     const std::string & outputType,
                                     const std::vector<FunctionArgument> & arguments = {},
                                     const std::string & outputName = "") = 0;

        // 3MF and implicit modeling operations
        virtual bool validateDocumentFor3MF() const = 0;
        virtual bool exportDocumentAs3MF(const std::string & path,
                                         bool includeImplicitFunctions = true) const = 0;
        virtual std::pair<bool, uint32_t> createSDFFunction(const std::string & name,
                                                            const std::string & sdfExpression) = 0;
        virtual std::pair<bool, uint32_t>
        createCSGOperation(const std::string & name,
                           const std::string & operation,
                           const std::vector<std::string> & operands,
                           bool smooth = false,
                           float blendRadius = 0.1f) = 0;

        // 3MF Resource creation methods (return success flag and resource ID)
        virtual std::pair<bool, uint32_t> createLevelSet(uint32_t functionId) = 0;
        virtual std::pair<bool, uint32_t> createImage3DFunction(const std::string & name,
                                                                const std::string & imagePath,
                                                                float valueScale = 1.0f,
                                                                float valueOffset = 0.0f) = 0;
        virtual std::pair<bool, uint32_t> createVolumetricColor(uint32_t functionId,
                                                                const std::string & channel) = 0;
        virtual std::pair<bool, uint32_t> createVolumetricProperty(const std::string & propertyName,
                                                                   uint32_t functionId,
                                                                   const std::string & channel) = 0;
        virtual bool applyTransformToFunction(const std::string & functionName,
                                              const std::array<float, 3> & translation,
                                              const std::array<float, 3> & rotation,
                                              const std::array<float, 3> & scale) = 0;
        virtual nlohmann::json
        analyzeFunctionProperties(const std::string & functionName) const = 0;
        virtual nlohmann::json generateMeshFromFunction(
          const std::string & functionName,
          int resolution = 64,
          const std::array<float, 6> & bounds = {-10, -10, -10, 10, 10, 10}) const = 0;

        // Scene and hierarchy operations
        virtual nlohmann::json getSceneHierarchy() const = 0;
        virtual nlohmann::json getDocumentInfo() const = 0;
        virtual std::vector<std::string> listAvailableFunctions() const = 0;

        /**
         * @brief Get a comprehensive structure of the current 3MF model
         *
         * Returns a JSON object listing build items and resources (meshes, level sets,
         * functions, images, materials, etc.) to allow assistants to inspect
         * what is contained in the document.
         *
         * Expected JSON shape (fields may vary if information is unavailable):
         * {
         *   "has_document": bool,
         *   "document_path": string,
         *   "build_items": [ { ... } ],
         *   "resources": [ { ... } ],
         *   "counts": { "build_items": n, "resources": n, "meshes": n, ... }
         * }
         */
        virtual nlohmann::json get3MFStructure() const = 0;

        /**
         * @brief Serialize and return the node graph of a function (model) as JSON.
         *
         * The function can be addressed by its ModelResourceID (resource id in 3MF), which
         * corresponds to the model id used throughout Gladius. Returns an error if the
         * model does not exist.
         *
         * @param functionId ModelResourceID of the function (model) to serialize.
         * @return JSON with nodes, parameters, outputs and links.
         */
        virtual nlohmann::json getFunctionGraph(uint32_t functionId) const = 0;

        // Manufacturing validation
        virtual nlohmann::json
        validateForManufacturing(const std::vector<std::string> & functionNames = {},
                                 const nlohmann::json & constraints = {}) const = 0;

        // Build item and level set modification (authoring helpers)
        /**
         * @brief Set the referenced object (by ModelResourceID) on an existing build item.
         * @param buildItemIndex Zero-based index of the build item in the model's build list.
         * @param objectModelResourceId ModelResourceID of the object to reference (mesh,
         * components, levelset, ...).
         * @return true on success, false otherwise (check getLastErrorMessage()).
         */
        virtual bool setBuildItemObjectByIndex(uint32_t buildItemIndex,
                                               uint32_t objectModelResourceId) = 0;

        /**
         * @brief Set the transform of an existing build item.
         * @param buildItemIndex Zero-based index of the build item in the model's build list.
         * @param transform4x3RowMajor 12 floats (row-major 4x3 matrix) matching Lib3MF::sTransform
         * fields.
         * @return true on success, false otherwise (check getLastErrorMessage()).
         */
        virtual bool
        setBuildItemTransformByIndex(uint32_t buildItemIndex,
                                     const std::array<float, 12> & transform4x3RowMajor) = 0;

        /**
         * @brief Modify a level set's referenced function and/or output channel.
         * @param levelSetModelResourceId ModelResourceID of the level set to modify.
         * @param functionModelResourceId Optional ModelResourceID of the function to reference.
         * @param channel Optional channel/output name to set (e.g., "shape" or "result").
         * @return true on success, false otherwise (check getLastErrorMessage()).
         */
        virtual bool modifyLevelSet(uint32_t levelSetModelResourceId,
                                    std::optional<uint32_t> functionModelResourceId,
                                    std::optional<std::string> channel) = 0;

        /**
         * @brief Validate the current model in two phases and return diagnostics.
         *
         * Phases:
         *  1) graph_sync: Update 3MF model and inputs/outputs, validate assembly structure.
         *  2) opencl_compile: Generate kernels and attempt an OpenCL build.
         *
         * Options (JSON):
         *  - compile (bool, default true): run the OpenCL compile phase.
         *  - max_messages (int, default 50): limit of diagnostic messages to include.
         *
         * Returns a JSON object with fields:
         * {
         *   success: bool,
         *   phases: [
         *     { name: "graph_sync", ok: bool, errors: n, warnings: n, messages: [...] },
         *     { name: "opencl_compile", ok: bool, errors: n, warnings: n, messages: [...] }
         *   ],
         *   summary: { graph_ok: bool, compile_ok: bool }
         * }
         */
        /**
         * @brief Optional: Validate the current model. Default returns a stub if not overridden.
         */
        virtual nlohmann::json validateModel(const nlohmann::json & options = {})
        {
            (void) options; // unused in default implementation
            nlohmann::json res;
            res["success"] = false;
            res["phases"] = nlohmann::json::array();
            res["summary"] = {{"graph_ok", false}, {"compile_ok", false}};
            res["error"] = "validateModel not implemented";
            return res;
        }

        // Rendering operations
        /**
         * @brief Render the current 3MF model to an image file
         * @param outputPath File path where to save the rendered image
         * @param width Image width in pixels (default: 1024)
         * @param height Image height in pixels (default: 1024)
         * @param format Output format ("png", "jpg") (default: "png")
         * @param quality Quality setting 0.0-1.0 for lossy formats (default: 0.9)
         * @return true on success, false otherwise
         */
        virtual bool renderToFile(const std::string & outputPath,
                                  uint32_t width = 1024,
                                  uint32_t height = 1024,
                                  const std::string & format = "png",
                                  float quality = 0.9f) = 0;

        /**
         * @brief Render with camera settings
         * @param outputPath File path where to save the rendered image
         * @param cameraSettings JSON object with camera parameters:
         *   - eye_position: [x, y, z] camera position
         *   - target_position: [x, y, z] look-at target
         *   - up_vector: [x, y, z] up direction (default: [0, 0, 1])
         *   - field_of_view: degrees (default: 45.0)
         * @param renderSettings JSON object with render parameters:
         *   - width: image width in pixels (default: 1024)
         *   - height: image height in pixels (default: 1024)
         *   - format: output format "png", "jpg" (default: "png")
         *   - quality: quality 0.0-1.0 for lossy formats (default: 0.9)
         *   - background_color: [r, g, b, a] normalized (default: [0.2, 0.2, 0.2, 1.0])
         *   - enable_shadows: boolean (default: true)
         *   - enable_lighting: boolean (default: true)
         * @return true on success, false otherwise
         */
        virtual bool renderWithCamera(const std::string & outputPath,
                                      const nlohmann::json & cameraSettings = {},
                                      const nlohmann::json & renderSettings = {}) = 0;

        /**
         * @brief Generate a thumbnail image of the current model
         * @param outputPath File path where to save the thumbnail
         * @param size Thumbnail size in pixels (square image) (default: 256)
         * @return true on success, false otherwise
         */
        virtual bool generateThumbnail(const std::string & outputPath, uint32_t size = 256) = 0;

        /**
         * @brief Get optimal camera position for the current model
         * @return JSON object with suggested camera settings for best view
         */
        virtual nlohmann::json getOptimalCameraPosition() const = 0;

        // Batch operations
        virtual bool executeBatchOperations(const nlohmann::json & operations,
                                            bool rollbackOnError = true) = 0;

        // Error handling
        virtual std::string getLastErrorMessage() const = 0;
    };
}
