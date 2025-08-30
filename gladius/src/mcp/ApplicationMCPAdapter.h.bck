/**
 * @file ApplicationMCPAdapter.h
 * @brief Adapter to make Application work with MCPApplicationInterface
 */

#pragma once

#include "MCPApplicationInterface.h"
#include <memory>
#include <nlohmann/json.hpp>

namespace gladius
{
    class Application; // Forward declaration

    namespace mcp
    {
        class CoroMCPAdapter; // Forward declaration
    }

    /**
     * @brief Adapter class to connect Application to MCP server
     * This allows the MCP server to use the minimal interface while still
     * connecting to the real Application class
     */
    class ApplicationMCPAdapter : public MCPApplicationInterface
    {
      private:
        Application * m_application;            // Raw pointer to avoid circular dependencies
        mutable std::string m_lastErrorMessage; // Store detailed error information
        std::unique_ptr<mcp::CoroMCPAdapter> m_coroAdapter; // Coroutine-based async operations

        // Helper method to automatically validate after operations
        nlohmann::json performAutoValidation(bool includeOpenCL = false) const;

      public:
        explicit ApplicationMCPAdapter(Application * app);
        ~ApplicationMCPAdapter()
          override; // Custom destructor needed for unique_ptr<CoroMCPAdapter>

        // Get the last error message for debugging
        std::string getLastErrorMessage() const override
        {
            return m_lastErrorMessage;
        }

        // MCPApplicationInterface implementation
        std::string getVersion() const override;
        bool isRunning() const override;
        std::string getApplicationName() const override;
        std::string getStatus() const override;
        void setHeadlessMode(bool headless) override;
        bool isHeadlessMode() const override;
        bool showUI() override;
        bool isUIRunning() const override;
        bool hasActiveDocument() const override;
        std::string getActiveDocumentPath() const override;

        // Document lifecycle operations
        bool createNewDocument() override;
        bool openDocument(const std::string & path) override;
        bool saveDocument() override;
        bool saveDocumentAs(const std::string & path) override;
        bool exportDocument(const std::string & path, const std::string & format) override;

        // Parameter operations
        bool setFloatParameter(uint32_t modelId,
                               const std::string & nodeName,
                               const std::string & parameterName,
                               float value) override;
        float getFloatParameter(uint32_t modelId,
                                const std::string & nodeName,
                                const std::string & parameterName) override;
        bool setStringParameter(uint32_t modelId,
                                const std::string & nodeName,
                                const std::string & parameterName,
                                const std::string & value) override;
        std::string getStringParameter(uint32_t modelId,
                                       const std::string & nodeName,
                                       const std::string & parameterName) override;

        // Expression and function operations
        std::pair<bool, uint32_t>
        createFunctionFromExpression(const std::string & name,
                                     const std::string & expression,
                                     const std::string & outputType,
                                     const std::vector<FunctionArgument> & arguments = {},
                                     const std::string & outputName = "") override;

        // 3MF and implicit modeling operations
        bool validateDocumentFor3MF() const override;
        bool exportDocumentAs3MF(const std::string & path,
                                 bool includeImplicitFunctions = true) const override;
        std::pair<bool, uint32_t> createSDFFunction(const std::string & name,
                                                    const std::string & sdfExpression) override;
        std::pair<bool, uint32_t> createCSGOperation(const std::string & name,
                                                     const std::string & operation,
                                                     const std::vector<std::string> & operands,
                                                     bool smooth = false,
                                                     float blendRadius = 0.1f) override;

        // 3MF Resource creation methods (return success flag and resource ID)
        std::pair<bool, uint32_t> createLevelSet(uint32_t functionId) override;
        std::pair<bool, uint32_t> createImage3DFunction(const std::string & name,
                                                        const std::string & imagePath,
                                                        float valueScale = 1.0f,
                                                        float valueOffset = 0.0f) override;
        std::pair<bool, uint32_t> createVolumetricColor(uint32_t functionId,
                                                        const std::string & channel) override;
        std::pair<bool, uint32_t> createVolumetricProperty(const std::string & propertyName,
                                                           uint32_t functionId,
                                                           const std::string & channel) override;
        bool applyTransformToFunction(const std::string & functionName,
                                      const std::array<float, 3> & translation,
                                      const std::array<float, 3> & rotation,
                                      const std::array<float, 3> & scale) override;
        nlohmann::json analyzeFunctionProperties(const std::string & functionName) const override;
        nlohmann::json generateMeshFromFunction(
          const std::string & functionName,
          int resolution = 64,
          const std::array<float, 6> & bounds = {-10, -10, -10, 10, 10, 10}) const override;

        // Scene and hierarchy operations
        nlohmann::json getSceneHierarchy() const override;
        nlohmann::json getDocumentInfo() const override;
        std::vector<std::string> listAvailableFunctions() const override;
        nlohmann::json get3MFStructure() const override;
        nlohmann::json getFunctionGraph(uint32_t functionId) const override;
        nlohmann::json getNodeInfo(uint32_t functionId, uint32_t nodeId) const override;
        nlohmann::json createNode(uint32_t functionId,
                                  const std::string & nodeType,
                                  const std::string & displayName,
                                  uint32_t nodeId) override;
        nlohmann::json deleteNode(uint32_t functionId, uint32_t nodeId) override;
        nlohmann::json setParameterValue(uint32_t functionId,
                                         uint32_t nodeId,
                                         const std::string & parameterName,
                                         const nlohmann::json & value) override;
        nlohmann::json createLink(uint32_t functionId,
                                  uint32_t sourceNodeId,
                                  const std::string & sourcePortName,
                                  uint32_t targetNodeId,
                                  const std::string & targetParameterName) override;
        nlohmann::json deleteLink(uint32_t functionId,
                                  uint32_t targetNodeId,
                                  const std::string & targetParameterName) override;

        // Manufacturing validation
        nlohmann::json
        validateForManufacturing(const std::vector<std::string> & functionNames = {},
                                 const nlohmann::json & constraints = {}) const override;

        // Rendering operations
        bool renderToFile(const std::string & outputPath,
                          uint32_t width = 1024,
                          uint32_t height = 1024,
                          const std::string & format = "png",
                          float quality = 0.9f) override;
        bool renderWithCamera(const std::string & outputPath,
                              const nlohmann::json & cameraSettings = {},
                              const nlohmann::json & renderSettings = {}) override;
        bool generateThumbnail(const std::string & outputPath, uint32_t size = 256) override;
        nlohmann::json getOptimalCameraPosition() const override;

        // Bounding box query
        nlohmann::json getModelBoundingBox() const override;

        // Resource cleanup
        nlohmann::json removeUnusedResources() override;

        // Batch operations
        bool executeBatchOperations(const nlohmann::json & operations,
                                    bool rollbackOnError = true) override;

        // Two-phase validation (graph sync + OpenCL compile)
        nlohmann::json validateModel(const nlohmann::json & options = {}) override;

        // Build item and level set modification
        bool setBuildItemObjectByIndex(uint32_t buildItemIndex,
                                       uint32_t objectModelResourceId) override;
        bool
        setBuildItemTransformByIndex(uint32_t buildItemIndex,
                                     const std::array<float, 12> & transform4x3RowMajor) override;
        bool modifyLevelSet(uint32_t levelSetModelResourceId,
                            std::optional<uint32_t> functionModelResourceId,
                            std::optional<std::string> channel) override;
    };
}
