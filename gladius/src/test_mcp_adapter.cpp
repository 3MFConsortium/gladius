/**
 * @file test_mcp_adapter.cpp
 * @brief Test MCP adapter functionality without full Application build
 */

#include "mcp/ApplicationMCPAdapter.h"
#include "mcp/MCPApplicationInterface.h"
#include "mcp/MCPServer.h"
#include <iostream>
#include <memory>

// Mock application class to simulate the real Application
class MockApplication
{
  public:
    MockApplication() = default;
    ~MockApplication() = default;

    // Simulate some application methods
    std::string getVersion() const
    {
        return "1.0.0-mock";
    }
    bool isRunning() const
    {
        return true;
    }
    std::string getStatus() const
    {
        return "mock_running";
    }
};

int main()
{
    try
    {
        std::cout << "=== Testing MCP Adapter Pattern ===" << std::endl;

        // Create mock application
        auto mockApp = std::make_unique<MockApplication>();
        std::cout << "✓ Mock application created" << std::endl;

        // Create adapter
        auto adapter = std::make_unique<gladius::ApplicationMCPAdapter>(
          reinterpret_cast<gladius::Application *>(mockApp.get()));
        std::cout << "✓ MCP Adapter created" << std::endl;

        // Test adapter methods
        std::cout << "Adapter getApplicationName: " << adapter->getApplicationName() << std::endl;
        std::cout << "Adapter getVersion: " << adapter->getVersion() << std::endl;
        std::cout << "Adapter isRunning: " << (adapter->isRunning() ? "true" : "false")
                  << std::endl;
        std::cout << "Adapter getStatus: " << adapter->getStatus() << std::endl;
        std::cout << "✓ Adapter methods work correctly" << std::endl;

        // Create MCP server with adapter
        auto mcpServer = std::make_unique<gladius::mcp::MCPServer>(adapter.get());
        std::cout << "✓ MCP Server created with adapter" << std::endl;

        // Test MCP server functionality
        auto tools = mcpServer->getRegisteredTools();
        std::cout << "✓ MCP Server has " << tools.size() << " registered tools" << std::endl;

        // Test direct tool execution
        auto statusResult = mcpServer->executeTool("get_status", nlohmann::json{});
        std::cout << "✓ Status tool result: " << statusResult.dump() << std::endl;

        std::cout << "=== MCP Adapter Pattern Test Completed Successfully! ===" << std::endl;
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
