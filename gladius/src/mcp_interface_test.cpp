/**
 * @file mcp_interface_test.cpp
 * @brief Test MCP server with minimal interface (no heavy Application dependencies)
 */

#include "mcp/MCPApplicationInterface.h"
#include "mcp/MCPServer.h"
#include <iostream>
#include <memory>

// Test implementation of the interface
class TestApplication : public gladius::MCPApplicationInterface
{
  public:
    std::string getVersion() const override
    {
        return "1.0.0-test";
    }
    bool isRunning() const override
    {
        return true;
    }
    std::string getApplicationName() const override
    {
        return "Gladius";
    }
    std::string getStatus() const override
    {
        return "running";
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
    std::cout << "=== MCP Interface Test ===" << std::endl;

    try
    {
        auto app = std::make_shared<TestApplication>();
        auto mcpServer = std::make_unique<gladius::mcp::MCPServer>(app);

        std::cout << "✓ MCP Server created with interface" << std::endl;

        // Start server
        bool started = mcpServer->start(8080);
        std::cout << "✓ MCP Server start result: " << (started ? "Success" : "Failed") << std::endl;

        if (started)
        {
            // Test tools
            auto statusResult = mcpServer->executeTool("get_status", {});
            std::cout << "✓ Status test result: " << statusResult.dump() << std::endl;

            auto tools = mcpServer->getRegisteredTools();
            std::cout << "✓ Available tools: " << tools.size() << std::endl;

            std::cout << "\n✓ MCP Server running on http://localhost:8080" << std::endl;
            std::cout << "✓ Press Enter to stop..." << std::endl;
            std::cin.get();

            mcpServer->stop();
        }

        std::cout << "=== MCP Interface test completed! ===" << std::endl;
        return 0;
    }
    catch (const std::exception & e)
    {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
