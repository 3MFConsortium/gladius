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

    /**
     * @brief Adapter class to connect Application to MCP server
     * This allows the MCP server to use the minimal interface while still
     * connecting to the real Application class
     */
    class ApplicationMCPAdapter : public MCPApplicationInterface
    {
      private:
        Application * m_application; // Raw pointer to avoid circular dependencies

      public:
        explicit ApplicationMCPAdapter(Application * app);
        ~ApplicationMCPAdapter() override = default;

        // MCPApplicationInterface implementation
        std::string getVersion() const override;
        bool isRunning() const override;
        std::string getApplicationName() const override;
        std::string getStatus() const override;
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
        bool createFunctionFromExpression(const std::string & name,
                                          const std::string & expression,
                                          const std::string & outputType) override;
    };
}
