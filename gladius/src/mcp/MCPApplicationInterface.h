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

        // Document operations (for future expansion)
        virtual bool hasActiveDocument() const = 0;
        virtual std::string getActiveDocumentPath() const = 0;

        // Future: Add more specific operations as needed
        // virtual bool loadDocument(const std::string& path) = 0;
        // virtual bool saveDocument(const std::string& path) = 0;
    };
}
