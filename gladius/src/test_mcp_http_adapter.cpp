/**
 * @file test_mcp_http_adapter.cpp
 * @brief Test MCP HTTP server with adapter pattern
 */

#include "mcp/MCPApplicationInterface.h"
#include "mcp/MCPServer.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

// Simple mock implementation
class MockMCPInterface : public gladius::MCPApplicationInterface
{
  public:
    std::string getVersion() const override
    {
        return "1.0.0-adapter";
    }
    bool isRunning() const override
    {
        return true;
    }
    std::string getApplicationName() const override
    {
        return "AdapterGladius";
    }
    std::string getStatus() const override
    {
        return "adapter_running";
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
        std::cout << "=== Testing MCP HTTP Server with Adapter Pattern ===" << std::endl;

        // Create mock interface
        auto mockInterface = std::make_unique<MockMCPInterface>();
        std::cout << "✓ Mock interface created" << std::endl;

        // Create MCP server with raw pointer (adapter pattern)
        auto mcpServer = std::make_unique<gladius::mcp::MCPServer>(mockInterface.get());
        std::cout << "✓ MCP Server created with adapter pattern" << std::endl;

        // Test tools before starting HTTP server
        auto tools = mcpServer->getRegisteredTools();
        std::cout << "✓ MCP Server has " << tools.size() << " registered tools" << std::endl;

        // Start HTTP server on port 8081
        std::cout << "Starting HTTP server on port 8081..." << std::endl;
        bool started = mcpServer->start(8081);

        if (!started)
        {
            std::cout << "❌ Failed to start HTTP server" << std::endl;
            return 1;
        }

        std::cout << "✓ HTTP Server started successfully" << std::endl;
        std::cout << "✓ Server running: " << (mcpServer->isRunning() ? "true" : "false")
                  << std::endl;

        // Give the server a moment to start
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Test JSON-RPC call using curl (simple test)
        std::cout << "✓ HTTP server is available at http://localhost:8081" << std::endl;
        std::cout << "✓ Test the server with:" << std::endl;
        std::cout << "   curl -X POST http://localhost:8081/mcp \\" << std::endl;
        std::cout << "     -H \"Content-Type: application/json\" \\" << std::endl;
        std::cout << "     -d "
                     "'{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/"
                     "call\",\"params\":{\"name\":\"get_status\",\"arguments\":{}}}'"
                  << std::endl;

        // Keep running for a short time to allow testing
        std::cout << "\nServer will run for 5 seconds for testing..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Stop the server
        std::cout << "Stopping server..." << std::endl;
        mcpServer->stop();
        std::cout << "✓ Server stopped" << std::endl;

        std::cout << "=== MCP HTTP Adapter Test Completed Successfully! ===" << std::endl;
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
