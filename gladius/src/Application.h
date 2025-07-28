#pragma once

#include "ConfigManager.h"
#include "ui/MainWindow.h"
#include <memory>

namespace gladius::mcp
{
    class MCPServer;
    class ApplicationMCPAdapter;
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
         * @brief Disable MCP server
         */
        void disableMCPServer();

        /**
         * @brief Check if MCP server is enabled and running
         * @return true if MCP server is running
         */
        bool isMCPServerEnabled() const;

        /**
         * @brief Get the main window reference
         * @return Reference to the main window
         */
        ui::MainWindow & getMainWindow()
        {
            return m_mainWindow;
        }

      private:
        ConfigManager m_configManager;
        ui::MainWindow m_mainWindow;
        std::unique_ptr<mcp::MCPServer> m_mcpServer;
        std::unique_ptr<mcp::ApplicationMCPAdapter> m_mcpAdapter;
    };
}
