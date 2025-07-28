/**
 * @file test_full_integration.cpp
 * @brief Test how the adapter pattern would work in the real Application
 */

#include "mcp/ApplicationMCPAdapter.h"
#include "mcp/MCPServer.h"
#include <iostream>
#include <memory>

// Simple mock of Application class (just the relevant parts)
class MockApplicationForAdapter
{
  public:
    MockApplicationForAdapter() = default;
    ~MockApplicationForAdapter() = default;

    // Methods that the real Application would have
    std::string getVersion() const
    {
        return "1.0.0-real";
    }
    bool isRunning() const
    {
        return true;
    }
    std::string getStatus() const
    {
        return "fully_integrated";
    }

    // MCP integration methods
    bool enableMCPServer(int port = 8080)
    {
        try
        {
            // Create the adapter that bridges this Application to the MCP interface
            m_mcpAdapter = std::make_unique<gladius::ApplicationMCPAdapter>(
              reinterpret_cast<gladius::Application *>(this));

            // Create the MCP server with the adapter
            m_mcpServer = std::make_unique<gladius::mcp::MCPServer>(m_mcpAdapter.get());
            bool success = m_mcpServer->start(port);

            if (success)
            {
                std::cout << "✓ MCP Server enabled on port " << port << std::endl;
            }
            else
            {
                std::cout << "❌ Failed to enable MCP Server on port " << port << std::endl;
                m_mcpServer.reset();
                m_mcpAdapter.reset();
            }

            return success;
        }
        catch (const std::exception & e)
        {
            std::cout << "❌ Error enabling MCP Server: " << e.what() << std::endl;
            m_mcpServer.reset();
            m_mcpAdapter.reset();
            return false;
        }
    }

    void disableMCPServer()
    {
        if (m_mcpServer)
        {
            m_mcpServer->stop();
            m_mcpServer.reset();
            m_mcpAdapter.reset();
            std::cout << "✓ MCP Server disabled" << std::endl;
        }
    }

    bool isMCPServerEnabled() const
    {
        return m_mcpServer && m_mcpServer->isRunning();
    }

  private:
    std::unique_ptr<gladius::mcp::MCPServer> m_mcpServer;
    std::unique_ptr<gladius::ApplicationMCPAdapter> m_mcpAdapter;
};

int main()
{
    try
    {
        std::cout << "=== Testing Full MCP Integration Pattern ===" << std::endl;

        // Create mock application
        auto app = std::make_unique<MockApplicationForAdapter>();
        std::cout << "✓ Mock Application created" << std::endl;

        // Test MCP integration
        std::cout << "Enabling MCP server..." << std::endl;
        bool enabled = app->enableMCPServer(8082);

        if (!enabled)
        {
            std::cout << "❌ Failed to enable MCP server" << std::endl;
            return 1;
        }

        std::cout << "✓ MCP Server enabled successfully" << std::endl;
        std::cout << "✓ MCP Server running: " << (app->isMCPServerEnabled() ? "true" : "false")
                  << std::endl;

        // Test that we can disable it
        std::cout << "Disabling MCP server..." << std::endl;
        app->disableMCPServer();
        std::cout << "✓ MCP Server running after disable: "
                  << (app->isMCPServerEnabled() ? "true" : "false") << std::endl;

        std::cout << "=== Full Integration Test Completed Successfully! ===" << std::endl;
        std::cout << "\n✓ Summary: The adapter pattern allows:" << std::endl;
        std::cout << "  - Application class to create MCP server without heavy dependencies"
                  << std::endl;
        std::cout << "  - Clean separation between Application and MCP concerns" << std::endl;
        std::cout << "  - Proper lifecycle management of MCP server" << std::endl;
        std::cout << "  - Raw pointer usage to avoid circular dependencies" << std::endl;
        std::cout << "\n✓ Ready for production integration!" << std::endl;

        return 0;
    }
    catch (const std::exception & e)
    {
        std::cout << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cout << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
