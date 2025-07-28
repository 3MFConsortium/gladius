/**
 * @file test_mcp_minimal.cpp
 * @brief Minimal test of MCP server functionality without full dependencies
 */

#include "mcp/MCPApplicationInterface.h"
#include "mcp/MCPServer.h"
#include <iostream>
#include <memory>

// Simple mock implementation of the interface
class MockMCPInterface : public gladius::MCPApplicationInterface
{
  public:
    std::string getVersion() const override
    {
        return "1.0.0-mock";
    }
    bool isRunning() const override
    {
        return true;
    }
    std::string getApplicationName() const override
    {
        return "MockGladius";
    }
    std::string getStatus() const override
    {
        return "mock_running";
    }
    bool hasActiveDocument() const override
    {
        return false;
    }
    std::string getActiveDocumentPath() const override
    {
        return "";
    }
};

int main()
{
    try
    {
        std::cout << "=== Testing MCP Server with Raw Pointer ===" << std::endl;

        // Create mock interface
        auto mockInterface = std::make_unique<MockMCPInterface>();
        std::cout << "✓ Mock interface created" << std::endl;

        // Create MCP server with raw pointer constructor
        auto mcpServer = std::make_unique<gladius::mcp::MCPServer>(mockInterface.get());
        std::cout << "✓ MCP Server created with raw pointer" << std::endl;

        // Test MCP server functionality
        auto tools = mcpServer->getRegisteredTools();
        std::cout << "✓ MCP Server has " << tools.size() << " registered tools" << std::endl;

        // List tools
        for (const auto & tool : tools)
        {
            std::cout << "  - " << tool.name << ": " << tool.description << std::endl;
        }

        // Test direct tool execution
        auto statusResult = mcpServer->executeTool("get_status", nlohmann::json{});
        std::cout << "✓ Status tool result: " << statusResult.dump() << std::endl;

        // Test the mock interface directly
        std::cout << "✓ Mock interface methods:" << std::endl;
        std::cout << "  - Application: " << mockInterface->getApplicationName() << std::endl;
        std::cout << "  - Version: " << mockInterface->getVersion() << std::endl;
        std::cout << "  - Status: " << mockInterface->getStatus() << std::endl;
        std::cout << "  - Running: " << (mockInterface->isRunning() ? "true" : "false")
                  << std::endl;

        std::cout << "=== MCP Raw Pointer Test Completed Successfully! ===" << std::endl;
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
