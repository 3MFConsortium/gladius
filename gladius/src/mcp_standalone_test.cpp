/**
 * @file mcp_standalone_test.cpp
 * @brief Standalone test for MCP server functionality
 */

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Include only the necessary headers directly
#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace gladius::mcp
{
    // Forward declare a minimal Application interface
    class MockApplication
    {
      public:
        MockApplication() = default;
        ~MockApplication() = default;

        std::string getVersion() const
        {
            return "1.0.0";
        }
        bool isRunning() const
        {
            return true;
        }
    };

    using ToolFunction = std::function<json(const json &)>;

    struct ToolInfo
    {
        std::string name;
        std::string description;
        json schema;
    };

    class MCPServer
    {
      private:
        std::shared_ptr<MockApplication> m_application;
        std::map<std::string, ToolInfo> m_toolInfo;
        std::map<std::string, ToolFunction> m_tools;
        std::atomic<bool> m_running{false};
        int m_port{0};

        // HTTP server components
        std::unique_ptr<httplib::Server> m_server;
        std::thread m_serverThread;

      public:
        MCPServer(std::shared_ptr<MockApplication> app)
            : m_application(std::move(app))
            , m_server(std::make_unique<httplib::Server>())
        {
            setupRoutes();
            setupBuiltinTools();
            std::cout << "Standalone MCP Server initialized" << std::endl;
        }

        ~MCPServer()
        {
            stop();
        }

        void registerTool(const std::string & name,
                          const std::string & description,
                          const json & schema,
                          ToolFunction func)
        {
            m_toolInfo[name] = {name, description, schema};
            m_tools[name] = std::move(func);
        }

        bool start(int port = 8080)
        {
            if (m_running)
            {
                std::cout << "MCP Server is already running" << std::endl;
                return false;
            }

            m_port = port;

            // Start server in a separate thread
            m_serverThread = std::thread(
              [this, port]()
              {
                  m_running = true;
                  std::cout << "MCP Server starting on port " << port << std::endl;

                  if (!m_server->listen("localhost", port))
                  {
                      std::cerr << "Failed to start MCP server on port " << port << std::endl;
                      m_running = false;
                  }
              });

            // Give the server a moment to start
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            return m_running;
        }

        void stop()
        {
            if (!m_running)
            {
                return;
            }

            m_running = false;
            m_server->stop();

            if (m_serverThread.joinable())
            {
                m_serverThread.join();
            }

            m_port = 0;
            std::cout << "MCP Server stopped" << std::endl;
        }

        bool isRunning() const
        {
            return m_running;
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

        json executeTool(const std::string & toolName, const json & params)
        {
            auto toolIt = m_tools.find(toolName);
            if (toolIt == m_tools.end())
            {
                return {{"error", "Tool not found: " + toolName}};
            }

            try
            {
                return toolIt->second(params);
            }
            catch (const std::exception & e)
            {
                return {{"error", "Tool execution failed: " + std::string(e.what())}};
            }
        }

      private:
        void setupRoutes()
        {
            // Enable CORS for web-based MCP clients
            m_server->set_default_headers({{"Access-Control-Allow-Origin", "*"},
                                           {"Access-Control-Allow-Methods", "POST, GET, OPTIONS"},
                                           {"Access-Control-Allow-Headers", "Content-Type"}});

            // Handle OPTIONS requests for CORS
            m_server->Options("/.*",
                              [](const httplib::Request &, httplib::Response & res) { return; });

            // Main JSON-RPC endpoint
            m_server->Post("/",
                           [this](const httplib::Request & req, httplib::Response & res)
                           { handleJSONRPC(req, res); });

            // Health check endpoint
            m_server->Get("/health",
                          [this](const httplib::Request &, httplib::Response & res)
                          {
                              json response = {{"status", "ok"},
                                               {"server", "Gladius MCP Server"},
                                               {"running", m_running.load()},
                                               {"tools_count", m_tools.size()}};
                              res.set_content(response.dump(), "application/json");
                          });
        }

        void handleJSONRPC(const httplib::Request & req, httplib::Response & res)
        {
            try
            {
                json request = json::parse(req.body);
                json response;

                if (!request.contains("method"))
                {
                    response = createErrorResponse(0, -32600, "Invalid Request - missing method");
                }
                else
                {
                    std::string method = request["method"];
                    int id = request.value("id", 0);

                    if (method == "initialize")
                    {
                        response = handleInitialize(request);
                    }
                    else if (method == "tools/list")
                    {
                        response = handleListTools(request);
                    }
                    else if (method == "tools/call")
                    {
                        response = handleCallTool(request);
                    }
                    else
                    {
                        response = createErrorResponse(id, -32601, "Method not found: " + method);
                    }
                }

                res.set_content(response.dump(), "application/json");
            }
            catch (const json::parse_error & e)
            {
                json errorResponse =
                  createErrorResponse(0, -32700, "Parse error: " + std::string(e.what()));
                res.set_content(errorResponse.dump(), "application/json");
            }
            catch (const std::exception & e)
            {
                json errorResponse =
                  createErrorResponse(0, -32603, "Internal error: " + std::string(e.what()));
                res.set_content(errorResponse.dump(), "application/json");
            }
        }

        json handleInitialize(const json & request)
        {
            return {{"jsonrpc", "2.0"},
                    {"id", request.value("id", 0)},
                    {"result",
                     {{"protocolVersion", "2024-11-05"},
                      {"capabilities", {{"tools", json::object()}}},
                      {"serverInfo", {{"name", "Gladius MCP Server"}, {"version", "1.0.0"}}}}}};
        }

        json handleListTools(const json & request)
        {
            json tools = json::array();
            for (const auto & [name, info] : m_toolInfo)
            {
                tools.push_back({{"name", info.name},
                                 {"description", info.description},
                                 {"inputSchema", info.schema}});
            }

            return {
              {"jsonrpc", "2.0"}, {"id", request.value("id", 0)}, {"result", {{"tools", tools}}}};
        }

        json handleCallTool(const json & request)
        {
            try
            {
                if (!request.contains("params") || !request["params"].contains("name"))
                {
                    return createErrorResponse(
                      request.value("id", 0), -32602, "Invalid params - missing tool name");
                }

                std::string toolName = request["params"]["name"];
                auto toolIt = m_tools.find(toolName);

                if (toolIt == m_tools.end())
                {
                    return createErrorResponse(
                      request.value("id", 0), -32602, "Tool not found: " + toolName);
                }

                json params = request["params"].value("arguments", json::object());
                json result = toolIt->second(params);

                return {{"jsonrpc", "2.0"},
                        {"id", request.value("id", 0)},
                        {"result", {{"content", {{{"type", "text"}, {"text", result.dump()}}}}}}};
            }
            catch (const std::exception & e)
            {
                return createErrorResponse(
                  request.value("id", 0), -32603, "Tool execution error: " + std::string(e.what()));
            }
        }

        json createErrorResponse(int id, int code, const std::string & message) const
        {
            return {
              {"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
        }

        void setupBuiltinTools()
        {
            // Tool: ping
            registerTool(
              "ping",
              "Simple ping tool to test connectivity",
              {{"type", "object"},
               {"properties",
                {{"message",
                  {{"type", "string"}, {"description", "Optional message to echo back"}}}}},
               {"required", {}}},
              [](const json & params) -> json
              {
                  std::string message = params.value("message", "pong");
                  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
                  return {{"response", message}, {"timestamp", now}};
              });

            // Tool: get_status
            registerTool("get_status",
                         "Get the current status of the application",
                         {{"type", "object"}, {"properties", {}}, {"required", {}}},
                         [this](const json & params) -> json
                         {
                             return {{"status", "running"},
                                     {"application", "Gladius"},
                                     {"version", m_application->getVersion()},
                                     {"mcp_server_running", m_running.load()},
                                     {"available_tools", m_tools.size()}};
                         });

            // Tool: test_computation
            registerTool(
              "test_computation",
              "Test basic mathematical computation",
              {{"type", "object"},
               {"properties",
                {{"a", {{"type", "number"}, {"description", "First number"}}},
                 {"b", {{"type", "number"}, {"description", "Second number"}}},
                 {"operation",
                  {{"type", "string"},
                   {"enum", json::array({"add", "subtract", "multiply", "divide"})},
                   {"description", "Mathematical operation to perform"}}}}},
               {"required", json::array({"a", "b", "operation"})}},
              [](const json & params) -> json
              {
                  if (!params.contains("a") || !params.contains("b") ||
                      !params.contains("operation"))
                  {
                      return {{"error", "Missing required parameters: a, b, operation"}};
                  }

                  double a = params["a"];
                  double b = params["b"];
                  std::string op = params["operation"];
                  double result = 0.0;

                  if (op == "add")
                      result = a + b;
                  else if (op == "subtract")
                      result = a - b;
                  else if (op == "multiply")
                      result = a * b;
                  else if (op == "divide")
                  {
                      if (b == 0.0)
                          return {{"error", "Division by zero"}};
                      result = a / b;
                  }
                  else
                      return {{"error", "Invalid operation: " + op}};

                  return {{"result", result}, {"operation", op}, {"operands", json::array({a, b})}};
              });
        }
    };
}

int main()
{
    std::cout << "=== Standalone MCP Server Test ===" << std::endl;

    try
    {
        auto app = std::make_shared<gladius::mcp::MockApplication>();
        auto mcpServer = std::make_unique<gladius::mcp::MCPServer>(app);

        std::cout << "✓ MCP Server created" << std::endl;

        // Start server
        bool started = mcpServer->start(8080);
        std::cout << "✓ MCP Server start result: " << (started ? "Success" : "Failed") << std::endl;

        if (started)
        {
            // Test tool listing
            auto tools = mcpServer->getRegisteredTools();
            std::cout << "✓ Available tools: " << tools.size() << std::endl;
            for (const auto & tool : tools)
            {
                std::cout << "  - " << tool.name << ": " << tool.description << std::endl;
            }

            // Test tools
            auto pingResult = mcpServer->executeTool("ping", {{"message", "Hello MCP!"}});
            std::cout << "✓ Ping test result: " << pingResult.dump() << std::endl;

            auto statusResult = mcpServer->executeTool("get_status", {});
            std::cout << "✓ Status test result: " << statusResult.dump() << std::endl;

            auto computeResult = mcpServer->executeTool(
              "test_computation", {{"a", 10}, {"b", 5}, {"operation", "add"}});
            std::cout << "✓ Computation test result: " << computeResult.dump() << std::endl;

            std::cout << "\n✓ MCP Server is running on http://localhost:8080" << std::endl;
            std::cout << "✓ Try: curl -X POST http://localhost:8080/health" << std::endl;
            std::cout << "✓ Press Enter to stop..." << std::endl;
            std::cin.get();

            mcpServer->stop();
        }

        std::cout << "=== Standalone MCP Server test completed! ===" << std::endl;
        return 0;
    }
    catch (const std::exception & e)
    {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}
