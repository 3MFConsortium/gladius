/**
 * @file ApplicationMCPAdapter.h
 * @brief Adapter to make Application work with MCPApplicationInterface
 */

#pragma once

#include "mcp/MCPApplicationInterface.h"
#include <memory>

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
    };
}
