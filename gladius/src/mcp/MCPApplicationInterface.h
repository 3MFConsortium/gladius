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
        virtual std::pair<bool, uint32_t> createLevelSet(uint32_t functionId,
                                                         int meshResolution = 64) = 0;
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

        // Batch operations
        virtual bool executeBatchOperations(const nlohmann::json & operations,
                                            bool rollbackOnError = true) = 0;

        // Error handling
        virtual std::string getLastErrorMessage() const = 0;
    };
}
