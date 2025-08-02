/**
 * @file MCPApplicationInterface.h
 * @brief Minimal interface for MCP server to interact with Application
 */

#pragma once

#include <memory>
#include <string>

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
    };
}
