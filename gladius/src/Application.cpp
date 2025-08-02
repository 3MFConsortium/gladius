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
        , m_globalLogger(std::make_shared<events::Logger>())
        , m_mcpServer(nullptr)
        , m_mcpAdapter(nullptr)
    {
        m_mainWindow.setConfigManager(m_configManager);
        if (!m_headlessMode)
        {
            m_mainWindow.setup();
        }
    }

    Application::Application(int argc, char ** argv)
        : m_configManager()
        , m_mainWindow()
        , m_globalLogger(std::make_shared<events::Logger>())
        , m_mcpServer(nullptr)
        , m_mcpAdapter(nullptr)
    {
        m_mainWindow.setConfigManager(m_configManager);
        m_mainWindow.setup();

        // the first argument is the executable name
        if (argc >= 2)
        {
            std::filesystem::path filename{argv[1]};

            // Use the global logger instead of cout
            m_globalLogger->logInfo("Opening file: " + filename.string());
            if (std::filesystem::exists(filename))
            {
                m_mainWindow.open(filename);
            }
            else
            {
                m_globalLogger->logError("File does not exist: " + filename.string());
            }
        }
        else
        {
            m_globalLogger->logInfo("No file specified");
        }
    }

    Application::Application(std::filesystem::path const & filename)
        : m_configManager()
        , m_mainWindow()
        , m_globalLogger(std::make_shared<events::Logger>())
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
            m_globalLogger->logError("File does not exist: " + filename.string());
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
            m_globalLogger->logWarning("MCP Server is already running");
            return false;
        }

        try
        {
            // Create the adapter that will bridge this Application to the MCP interface
            m_mcpAdapter = std::make_unique<ApplicationMCPAdapter>(this);

            // Create the MCP server with the adapter
            m_mcpServer = std::make_unique<mcp::MCPServer>(m_mcpAdapter.get());
            bool success = m_mcpServer->start(port, mcp::TransportType::HTTP);

            if (success)
            {
                m_globalLogger->logInfo("MCP Server enabled on port " + std::to_string(port));
            }
            else
            {
                m_globalLogger->logError("Failed to enable MCP Server on port " +
                                         std::to_string(port));
                m_mcpServer.reset();
                m_mcpAdapter.reset();
            }

            return success;
        }
        catch (const std::exception & e)
        {
            m_globalLogger->logError("Error enabling MCP Server: " + std::string(e.what()));
            m_mcpServer.reset();
            m_mcpAdapter.reset();
            return false;
        }
    }

    bool Application::enableMCPServerStdio()
    {
        if (m_mcpServer && m_mcpServer->isRunning())
        {
            return false; // Don't print anything in stdio mode
        }

        try
        {
            // Create the adapter that will bridge this Application to the MCP interface
            m_mcpAdapter = std::make_unique<ApplicationMCPAdapter>(this);

            // Create the MCP server with the adapter
            m_mcpServer = std::make_unique<mcp::MCPServer>(m_mcpAdapter.get());
            bool success = m_mcpServer->start(0, mcp::TransportType::STDIO);

            if (!success)
            {
                m_mcpServer.reset();
                m_mcpAdapter.reset();
            }

            return success;
        }
        catch (const std::exception & e)
        {
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
            m_globalLogger->logInfo("MCP Server disabled");
        }
    }

    bool Application::isMCPServerEnabled() const
    {
        return m_mcpServer && m_mcpServer->isRunning();
    }

    events::SharedLogger Application::getGlobalLogger() const
    {
        return m_globalLogger;
    }

    void Application::setLoggerOutputMode(events::OutputMode mode)
    {
        if (m_globalLogger)
        {
            m_globalLogger->setOutputMode(mode);
        }
    }

    void Application::setHeadlessMode(bool headless)
    {
        m_headlessMode = headless;
    }

    bool Application::isHeadlessMode() const
    {
        return m_headlessMode;
    }
}
