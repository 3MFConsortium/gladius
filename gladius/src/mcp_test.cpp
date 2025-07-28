/**
 * @file mcp_test.cpp
 * @brief Simple test program for MCP server functionality (MVP version)
 */

#include "Application.h"
#include "mcp/MCPServer.h"
#include <iostream>
#include <memory>

using namespace gladius::mcp;

int main(int argc, char * argv[])
{
    std::cout << "=== MCP Server MVP Test ===" << std::endl;

    try
    {
        // Create a minimal application instance for testing
        auto app = std::make_shared<gladius::Application>();
        std::cout << "✓ Application created" << std::endl;

        // Create MCP server
        auto mcpServer = std::make_unique<MCPServer>(app);
        std::cout << "✓ MCP Server created" << std::endl;

        // Start server (MVP mode - no actual HTTP server)
        bool started = mcpServer->start(8080);
        std::cout << "✓ MCP Server start result: " << (started ? "Success" : "Failed") << std::endl;

        // Test tool listing
        auto tools = mcpServer->getRegisteredTools();
        std::cout << "✓ Available tools: " << tools.size() << std::endl;
        for (const auto & tool : tools)
        {
            std::cout << "  - " << tool.name << ": " << tool.description << std::endl;
        }

        // Test ping tool
        nlohmann::json pingParams = {{"message", "Hello MCP!"}};
        auto pingResult = mcpServer->executeTool("ping", pingParams);
        std::cout << "✓ Ping test result: " << pingResult.dump() << std::endl;

        // Test status tool
        auto statusResult = mcpServer->executeTool("get_status", {});
        std::cout << "✓ Status test result: " << statusResult.dump() << std::endl;

        // Test computation tool
        nlohmann::json computeParams = {{"a", 10}, {"b", 5}, {"operation", "add"}};
        auto computeResult = mcpServer->executeTool("test_computation", computeParams);
        std::cout << "✓ Computation test result: " << computeResult.dump() << std::endl;

        // Test unknown tool
        auto unknownResult = mcpServer->executeTool("unknown_tool", {});
        std::cout << "✓ Unknown tool test result: " << unknownResult.dump() << std::endl;

        // Stop server
        mcpServer->stop();
        std::cout << "✓ MCP Server stopped" << std::endl;

        std::cout << "\n=== All MCP tests completed successfully! ===" << std::endl;
        return 0;
    }
    catch (const std::exception & e)
    {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
