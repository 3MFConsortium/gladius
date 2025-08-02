#pragma once

#include "ConfigManager.h"
#include "EventLogger.h"
#include "ui/MainWindow.h"
#include <memory>

namespace gladius
{
    class Document; // Forward declaration
}

namespace gladius::mcp
{
    class MCPServer;
}

namespace gladius
{
    class ApplicationMCPAdapter; // Forward declaration
}

namespace gladius
{
    class Application
    {
      public:
        Application();
        Application(int argc, char ** argv);
        Application(std::filesystem ::path const & filename);

        // Custom destructor needed for unique_ptr with forward declaration
        ~Application();

        /**
         * @brief Get the ConfigManager instance
         * @return Reference to the ConfigManager
         */
        ConfigManager & getConfigManager()
        {
            return m_configManager;
        }

        /**
         * @brief Get the ConfigManager instance (const version)
         * @return Const reference to the ConfigManager
         */
        ConfigManager const & getConfigManager() const
        {
            return m_configManager;
        }

        /**
         * @brief Enable MCP server on specified port
         * @param port Port to listen on (default: 8080)
         * @return true if server started successfully
         */
        bool enableMCPServer(int port = 8080);

        /**
         * @brief Enable MCP server with stdio transport (for VS Code)
         * @return true if server started successfully
         */
        bool enableMCPServerStdio();

        /**
         * @brief Enable headless mode (no GUI initialization)
         */
        void setHeadlessMode(bool headless);

        /**
         * @brief Check if running in headless mode
         * @return true if in headless mode
         */
        bool isHeadlessMode() const;

        /**
         * @brief Disable MCP server
         */
        void disableMCPServer();

        /**
         * @brief Check if MCP server is enabled and running
         * @return true if MCP server is running
         */
        bool isMCPServerEnabled() const;

        /**
         * @brief Get the global logger instance
         * @return Shared pointer to the global logger
         */
        events::SharedLogger getGlobalLogger() const;

        /**
         * @brief Set the global logger output mode
         * @param mode Output mode (Console or Silent)
         */
        void setLoggerOutputMode(events::OutputMode mode);

        /**
         * @brief Start the main application loop
         * This method blocks until the application exits
         */
        void startMainLoop();

        /**
         * @brief Get the main window reference
         * @return Reference to the main window
         */
        ui::MainWindow & getMainWindow()
        {
            return m_mainWindow;
        }

        /**
         * @brief Get the current active document
         * @return Shared pointer to the current document, or nullptr if none
         */
        std::shared_ptr<Document> getCurrentDocument() const;

      private:
        ConfigManager m_configManager;
        ui::MainWindow m_mainWindow;
        events::SharedLogger m_globalLogger;
        std::unique_ptr<mcp::MCPServer> m_mcpServer;
        std::unique_ptr<ApplicationMCPAdapter> m_mcpAdapter;
        bool m_headlessMode{false};
    };
}
