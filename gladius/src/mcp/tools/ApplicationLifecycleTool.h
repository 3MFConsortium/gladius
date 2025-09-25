/**
 * @file ApplicationLifecycleTool.h
 * @brief MCP tool for application lifecycle operations
 */

#pragma once

#include "MCPToolBase.h"
#include <string>

namespace gladius::mcp::tools
{
    /**
     * @brief MCP tool for application state and configuration management
     *
     * Handles application lifecycle operations such as version information,
     * running state, and UI management.
     */
    class ApplicationLifecycleTool : public MCPToolBase
    {
      public:
        /**
         * @brief Construct a new ApplicationLifecycleTool object
         * @param app Pointer to the Application instance
         */
        explicit ApplicationLifecycleTool(Application * app);

        /**
         * @brief Virtual destructor
         */
        virtual ~ApplicationLifecycleTool() = default;

        // Application information methods
        std::string getVersion() const;
        bool isRunning() const;
        std::string getApplicationName() const;
        std::string getStatus() const;

        // UI and mode management methods
        void setHeadlessMode(bool headless);
        bool isHeadlessMode() const;
        bool showUI();
        bool isUIRunning() const;
    };
} // namespace gladius::mcp::tools
