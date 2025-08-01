#include "Application.h"
#include "mcp/ApplicationMCPAdapter.h"
#include "mcp/MCPServer.h"
#include "ui/MainWindow.h"

#include <filesystem>
#include <iostream>

namespace gladius
{
    Application::Application()
        : m_configManager()
        , m_mainWindow()
        , m_mcpServer(nullptr)
        , m_mcpAdapter(nullptr)
    {
        m_mainWindow.setConfigManager(m_configManager);
        m_mainWindow.setup();
    }

    Application::Application(int argc, char ** argv)
        : m_configManager()
        , m_mainWindow()
        , m_mcpServer(nullptr)
        , m_mcpAdapter(nullptr)
    {
        m_mainWindow.setConfigManager(m_configManager);
        m_mainWindow.setup();

        // the first argument is the executable name
        if (argc >= 2)
        {
            std::filesystem::path filename{argv[1]};

            // because this is a console application, we print to the console
            std::cout << "Opening file: " << filename << std::endl;
            if (std::filesystem::exists(filename))
            {
                m_mainWindow.open(filename);
            }
            else
            {
                std::cout << "File does not exist: " << filename << std::endl;
            }
        }
        else
        {
            std::cout << "No file specified" << std::endl;
        }
    }

    Application::Application(std::filesystem::path const & filename)
        : m_configManager()
        , m_mainWindow()
        , m_mcpServer(nullptr)
        , m_mcpAdapter(nullptr)
    {
        m_mainWindow.setConfigManager(m_configManager);
        m_mainWindow.setup();

        if (std::filesystem::exists(filename))
        {
            m_mainWindow.open(filename);
        }
        else
        {
            std::cout << "File does not exist: " << filename << std::endl;
        }
    }

    void Application::startMainLoop()
    {
        m_mainWindow.startMainLoop();
    }

    Application::~Application()
    {
        // Default destructor implementation
        // The unique_ptr<MCPServer> will be properly destroyed here
        // since we have the complete MCPServer definition included
    }

    bool Application::enableMCPServer(int port)
    {
        if (m_mcpServer && m_mcpServer->isRunning())
        {
            std::cout << "MCP Server is already running" << std::endl;
            return false;
        }

        try
        {
            // Create the adapter that will bridge this Application to the MCP interface
            m_mcpAdapter = std::make_unique<ApplicationMCPAdapter>(this);

            // Create the MCP server with the adapter
            m_mcpServer = std::make_unique<mcp::MCPServer>(m_mcpAdapter.get());
            bool success = m_mcpServer->start(port);

            if (success)
            {
                std::cout << "MCP Server enabled on port " << port << std::endl;
            }
            else
            {
                std::cout << "Failed to enable MCP Server on port " << port << std::endl;
                m_mcpServer.reset();
                m_mcpAdapter.reset();
            }

            return success;
        }
        catch (const std::exception & e)
        {
            std::cout << "Error enabling MCP Server: " << e.what() << std::endl;
            m_mcpServer.reset();
            m_mcpAdapter.reset();
            return false;
        }
    }

    void Application::disableMCPServer()
    {
        if (m_mcpServer)
        {
            m_mcpServer->stop();
            m_mcpServer.reset();
            m_mcpAdapter.reset();
            std::cout << "MCP Server disabled" << std::endl;
        }
    }

    bool Application::isMCPServerEnabled() const
    {
        return m_mcpServer && m_mcpServer->isRunning();
    }
}
