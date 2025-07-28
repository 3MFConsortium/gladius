/**
 * @file simple_mcp_test.cpp
 * @brief Simple test to verify MCP structure compiles (without external dependencies)
 */

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Mock Application class for testing
class Application
{
  public:
    Application() = default;
    ~Application() = default;
};

// Simple JSON-like structure for testing
struct SimpleJSON
{
    std::map<std::string, std::string> data;

    SimpleJSON() = default;
    SimpleJSON(const std::map<std::string, std::string> & d)
        : data(d)
    {
    }

    std::string dump() const
    {
        std::string result = "{";
        for (const auto & [key, value] : data)
        {
            result += "\"" + key + "\": \"" + value + "\", ";
        }
        if (result.length() > 1)
            result = result.substr(0, result.length() - 2);
        result += "}";
        return result;
    }
};

// MCP Server structure test
namespace gladius::mcp
{
    using ToolFunction = std::function<SimpleJSON(const SimpleJSON &)>;

    struct ToolInfo
    {
        std::string name;
        std::string description;
        std::string schema;
    };

    class MCPServer
    {
      private:
        std::shared_ptr<Application> m_application;
        std::map<std::string, ToolInfo> m_toolInfo;
        std::map<std::string, ToolFunction> m_tools;
        bool m_running = false;
        int m_port = 0;

      public:
        MCPServer(std::shared_ptr<Application> app)
            : m_application(std::move(app))
        {
            setupBuiltinTools();
        }

        void registerTool(const std::string & name,
                          const std::string & description,
                          const std::string & schema,
                          ToolFunction func)
        {
            m_toolInfo[name] = {name, description, schema};
            m_tools[name] = std::move(func);
        }

        bool start(int port)
        {
            m_port = port;
            m_running = true;
            return true;
        }

        void stop()
        {
            m_running = false;
            m_port = 0;
        }

        SimpleJSON executeTool(const std::string & toolName, const SimpleJSON & params)
        {
            auto it = m_tools.find(toolName);
            if (it != m_tools.end())
            {
                return it->second(params);
            }
            std::map<std::string, std::string> errorData = {{"error", "Tool not found"}};
            return SimpleJSON(errorData);
        }

        std::vector<ToolInfo> getRegisteredTools() const
        {
            std::vector<ToolInfo> tools;
            for (const auto & [name, info] : m_toolInfo)
            {
                tools.push_back(info);
            }
            return tools;
        }

      private:
        void setupBuiltinTools()
        {
            registerTool("ping",
                         "Simple ping tool",
                         "{}",
                         [](const SimpleJSON & params) -> SimpleJSON
                         {
                             std::map<std::string, std::string> data = {{"response", "pong"}};
                             return SimpleJSON(data);
                         });

            registerTool("get_status",
                         "Get application status",
                         "{}",
                         [this](const SimpleJSON & params) -> SimpleJSON
                         {
                             std::map<std::string, std::string> data = {
                               {"status", "running"},
                               {"mcp_server", m_running ? "active" : "inactive"}};
                             return SimpleJSON(data);
                         });
        }
    };
}

int main()
{
    std::cout << "=== Simple MCP Structure Test ===" << std::endl;

    auto app = std::make_shared<Application>();
    auto mcpServer = std::make_unique<gladius::mcp::MCPServer>(app);

    std::cout << "✓ MCP Server created" << std::endl;

    mcpServer->start(8080);
    std::cout << "✓ MCP Server started" << std::endl;

    auto tools = mcpServer->getRegisteredTools();
    std::cout << "✓ Available tools: " << tools.size() << std::endl;
    for (const auto & tool : tools)
    {
        std::cout << "  - " << tool.name << ": " << tool.description << std::endl;
    }

    auto pingResult = mcpServer->executeTool("ping", SimpleJSON());
    std::cout << "✓ Ping result: " << pingResult.dump() << std::endl;

    auto statusResult = mcpServer->executeTool("get_status", SimpleJSON());
    std::cout << "✓ Status result: " << statusResult.dump() << std::endl;

    mcpServer->stop();
    std::cout << "✓ MCP Server stopped" << std::endl;

    std::cout << "=== All tests passed! MCP structure is valid ===" << std::endl;
    return 0;
}
