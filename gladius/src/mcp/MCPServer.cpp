/**
 * @file MCPServer.cpp
 * @brief Implementation of the MCP server for Gladius
 */

#include "MCPServer.h"
#include "MCPApplicationInterface.h"
#include <chrono>
#include <iostream>
#include <thread>

using json = nlohmann::json;

namespace gladius::mcp
{
    MCPServer::MCPServer(std::shared_ptr<MCPApplicationInterface> app)
        : m_application(app.get())
        , m_applicationShared(std::move(app))
        , m_server(std::make_unique<httplib::Server>())
    {
        setupRoutes();
        setupBuiltinTools();
        // Only print to stdout in HTTP mode
    }

    MCPServer::MCPServer(MCPApplicationInterface * app)
        : m_application(app)
        , m_applicationShared(nullptr)
        , m_server(std::make_unique<httplib::Server>())
    {
        setupRoutes();
        setupBuiltinTools();
        // Only print to stdout in HTTP mode
    }

    MCPServer::~MCPServer()
    {
        stop();
    }

    void MCPServer::registerTool(const std::string & name,
                                 const std::string & description,
                                 const nlohmann::json & schema,
                                 ToolFunction func)
    {
        m_toolInfo[name] = {name, description, schema};
        m_tools[name] = std::move(func);
        // Only log in HTTP mode, not stdio mode
        if (m_transportType == TransportType::HTTP)
        {
            std::cerr << "Registered MCP tool: " << name << std::endl;
        }
    }

    bool MCPServer::start(int port, TransportType transport)
    {
        if (m_running)
        {
            return false; // Don't print in case we're in stdio mode
        }

        m_transportType = transport;

        if (transport == TransportType::STDIO)
        {
            return startStdio();
        }
        else
        {
            return startHTTP(port);
        }
    }

    bool MCPServer::startStdio()
    {
        if (m_running)
        {
            return false;
        }

        m_running = true;

        // Start stdio processing in a separate thread
        m_stdioThread = std::thread([this]() { runStdioLoop(); });

        return true;
    }

    bool MCPServer::startHTTP(int port)
    {
        if (m_running)
        {
            std::cerr << "MCP Server is already running" << std::endl;
            return false;
        }

        m_port = port;

        // Start server in a separate thread
        m_serverThread = std::thread(
          [this, port]()
          {
              m_running = true;
              std::cerr << "MCP Server starting on port " << port << std::endl;

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

    void MCPServer::stop()
    {
        if (!m_running)
        {
            return;
        }

        m_running = false;

        if (m_transportType == TransportType::HTTP)
        {
            m_server->stop();
            if (m_serverThread.joinable())
            {
                m_serverThread.join();
            }
        }
        else if (m_transportType == TransportType::STDIO)
        {
            // For stdio, the thread will exit when m_running becomes false
            if (m_stdioThread.joinable())
            {
                m_stdioThread.join();
            }
        }

        m_port = 0;
        // Only print if not in stdio mode
        if (m_transportType == TransportType::HTTP)
        {
            std::cerr << "MCP Server stopped" << std::endl;
        }
    }

    bool MCPServer::isRunning() const
    {
        return m_running;
    }

    int MCPServer::getPort() const
    {
        return m_port;
    }

    std::vector<ToolInfo> MCPServer::getRegisteredTools() const
    {
        std::vector<ToolInfo> tools;
        for (const auto & [name, info] : m_toolInfo)
        {
            tools.push_back(info);
        }
        return tools;
    }

    nlohmann::json MCPServer::executeTool(const std::string & toolName,
                                          const nlohmann::json & params)
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

    void MCPServer::setupRoutes()
    {
        // Enable CORS for web-based MCP clients
        m_server->set_default_headers({{"Access-Control-Allow-Origin", "*"},
                                       {"Access-Control-Allow-Methods", "POST, GET, OPTIONS"},
                                       {"Access-Control-Allow-Headers", "Content-Type"}});

        // Handle OPTIONS requests for CORS
        m_server->Options("/.*", [](const httplib::Request &, httplib::Response & res) { return; });

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

    void MCPServer::handleJSONRPC(const httplib::Request & req, httplib::Response & res)
    {
        try
        {
            json request = json::parse(req.body);
            json response;

            // Check for required fields
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

    json MCPServer::handleInitialize(const json & request)
    {
        json response = {
          {"jsonrpc", "2.0"},
          {"id", request.value("id", 0)},
          {"result",
           {{"protocolVersion", "2024-11-05"},
            {"capabilities", {{"tools", json::object()}}},
            {"serverInfo", {{"name", "Gladius MCP Server"}, {"version", "1.0.0"}}}}}};

        return response;
    }

    json MCPServer::handleListTools(const json & request)
    {
        json tools = json::array();

        for (const auto & [name, info] : m_toolInfo)
        {
            tools.push_back({{"name", info.name},
                             {"description", info.description},
                             {"inputSchema", info.schema}});
        }

        json response = {{"jsonrpc", "2.0"}, {"result", {{"tools", tools}}}};

        if (request.contains("id"))
        {
            response["id"] = request["id"];
        }

        return response;
    }

    json MCPServer::handleCallTool(const json & request)
    {
        try
        {
            if (!request.contains("params") || !request["params"].contains("name"))
            {
                return createErrorResponse(request.contains("id") ? request["id"] : json(nullptr),
                                           -32602,
                                           "Invalid params - missing tool name");
            }

            std::string toolName = request["params"]["name"];
            auto toolIt = m_tools.find(toolName);

            if (toolIt == m_tools.end())
            {
                return createErrorResponse(request.contains("id") ? request["id"] : json(nullptr),
                                           -32601,
                                           "Tool not found: " + toolName);
            }

            json params = request["params"].value("arguments", json::object());
            json result = toolIt->second(params);

            json response = {
              {"jsonrpc", "2.0"},
              {"result", {{"content", {{{"type", "text"}, {"text", result.dump()}}}}}}};

            if (request.contains("id"))
            {
                response["id"] = request["id"];
            }

            return response;
        }
        catch (const std::exception & e)
        {
            return createErrorResponse(request.contains("id") ? request["id"] : json(nullptr),
                                       -32603,
                                       "Tool execution error: " + std::string(e.what()));
        }
    }

    json MCPServer::createErrorResponse(const nlohmann::json & id,
                                        int code,
                                        const std::string & message) const
    {
        return {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}};
    }

    void MCPServer::setupBuiltinTools()
    {
        // Tool: get_status - Get application status
        registerTool(
          "get_status",
          "Get the current status of Gladius application",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              return {{"status", m_application->getStatus()},
                      {"application", m_application->getApplicationName()},
                      {"version", m_application->getVersion()},
                      {"is_running", m_application->isRunning()},
                      {"mcp_server_running", m_running.load()},
                      {"available_tools", m_tools.size()},
                      {"has_active_document", m_application->hasActiveDocument()},
                      {"active_document_path", m_application->getActiveDocumentPath()},
                      {"timestamp",
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count()}};
          });

        // Tool: ping - Simple ping tool
        registerTool(
          "ping",
          "Simple ping tool to test connectivity",
          {{"type", "object"},
           {"properties",
            {{"message", {{"type", "string"}, {"description", "Optional message to echo back"}}}}},
           {"required", json::array()}},
          [](const json & params) -> json
          {
              std::string message = params.value("message", "pong");
              return {{"response", message},
                      {"timestamp",
                       std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count()}};
          });

        // Tool: list_tools - List available tools
        registerTool(
          "list_tools",
          "List all available MCP tools",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              json tools = json::array();
              for (const auto & [name, info] : m_toolInfo)
              {
                  tools.push_back({{"name", info.name},
                                   {"description", info.description},
                                   {"schema", info.schema}});
              }
              return {{"tools", tools}, {"count", tools.size()}};
          });

        // Tool: test_computation - Test basic computation
        registerTool("test_computation",
                     "Test basic mathematical computation",
                     {{"type", "object"},
                      {"properties",
                       {{"a", {{"type", "number"}, {"description", "First number"}}},
                        {"b", {{"type", "number"}, {"description", "Second number"}}},
                        {"operation",
                         {{"type", "string"},
                          {"enum", {"add", "subtract", "multiply", "divide"}},
                          {"description", "Mathematical operation to perform"}}}}},
                      {"required", {"a", "b", "operation"}}},
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
                         {
                             result = a + b;
                         }
                         else if (op == "subtract")
                         {
                             result = a - b;
                         }
                         else if (op == "multiply")
                         {
                             result = a * b;
                         }
                         else if (op == "divide")
                         {
                             if (b == 0.0)
                             {
                                 return {{"error", "Division by zero"}};
                             }
                             result = a / b;
                         }
                         else
                         {
                             return {{"error", "Invalid operation: " + op}};
                         }

                         return {{"result", result}, {"operation", op}, {"operands", {a, b}}};
                     });

        // Phase 1B: Document management tools
        registerTool(
          "create_document",
          "Create a new empty document",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              bool success = m_application->createNewDocument();
              return {{"success", success},
                      {"message", success ? "New document created" : "Failed to create document"}};
          });

        registerTool(
          "open_document",
          "Open a document from file",
          {{"type", "object"},
           {"properties",
            {{"path", {{"type", "string"}, {"description", "Path to the file to open"}}}}},
           {"required", {"path"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("path"))
              {
                  return {{"error", "Missing required parameter: path"}};
              }

              std::string path = params["path"];
              bool success = m_application->openDocument(path);
              return {
                {"success", success},
                {"path", path},
                {"message", success ? "Document opened successfully" : "Failed to open document"}};
          });

        registerTool(
          "save_document",
          "Save the current document",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              bool success = m_application->saveDocument();
              return {
                {"success", success},
                {"message", success ? "Document saved successfully" : "Failed to save document"}};
          });

        registerTool(
          "save_document_as",
          "Save the current document with a new filename",
          {{"type", "object"},
           {"properties",
            {{"path", {{"type", "string"}, {"description", "Path where to save the file"}}}}},
           {"required", {"path"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("path"))
              {
                  return {{"error", "Missing required parameter: path"}};
              }

              std::string path = params["path"];
              bool success = m_application->saveDocumentAs(path);
              return {
                {"success", success},
                {"path", path},
                {"message", success ? "Document saved successfully" : "Failed to save document"}};
          });

        registerTool(
          "export_document",
          "Export the document in a specific format",
          {{"type", "object"},
           {"properties",
            {{"path", {{"type", "string"}, {"description", "Path where to export the file"}}},
             {"format",
              {{"type", "string"}, {"enum", {"stl"}}, {"description", "Export format"}}}}},
           {"required", {"path", "format"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("path") || !params.contains("format"))
              {
                  return {{"error", "Missing required parameters: path, format"}};
              }

              std::string path = params["path"];
              std::string format = params["format"];
              bool success = m_application->exportDocument(path, format);
              return {{"success", success},
                      {"path", path},
                      {"format", format},
                      {"message",
                       success ? "Document exported successfully" : "Failed to export document"}};
          });

        // Phase 1C: Parameter management tools
        registerTool(
          "set_parameter",
          "Set a parameter value in the document",
          {{"type", "object"},
           {"properties",
            {{"model_id", {{"type", "integer"}, {"description", "Model ID"}}},
             {"node_name", {{"type", "string"}, {"description", "Node name"}}},
             {"parameter_name", {{"type", "string"}, {"description", "Parameter name"}}},
             {"value", {{"description", "Parameter value (number or string)"}}},
             {"type",
              {{"type", "string"},
               {"enum", {"float", "string"}},
               {"description", "Parameter type"}}}}},
           {"required", {"model_id", "node_name", "parameter_name", "value", "type"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("model_id") || !params.contains("node_name") ||
                  !params.contains("parameter_name") || !params.contains("value") ||
                  !params.contains("type"))
              {
                  return {{"error", "Missing required parameters"}};
              }

              uint32_t modelId = params["model_id"];
              std::string nodeName = params["node_name"];
              std::string parameterName = params["parameter_name"];
              std::string type = params["type"];

              bool success = false;
              json valueToReturn;
              if (type == "float")
              {
                  float value = params["value"];
                  valueToReturn = value;
                  success =
                    m_application->setFloatParameter(modelId, nodeName, parameterName, value);
              }
              else if (type == "string")
              {
                  std::string value = params["value"];
                  valueToReturn = value;
                  success =
                    m_application->setStringParameter(modelId, nodeName, parameterName, value);
              }
              else
              {
                  return {{"error", "Invalid parameter type: " + type}};
              }

              return {
                {"success", success},
                {"model_id", modelId},
                {"node_name", nodeName},
                {"parameter_name", parameterName},
                {"value", valueToReturn},
                {"type", type},
                {"message", success ? "Parameter set successfully" : "Failed to set parameter"}};
          });

        registerTool(
          "get_parameter",
          "Get a parameter value from the document",
          {{"type", "object"},
           {"properties",
            {{"model_id", {{"type", "integer"}, {"description", "Model ID"}}},
             {"node_name", {{"type", "string"}, {"description", "Node name"}}},
             {"parameter_name", {{"type", "string"}, {"description", "Parameter name"}}},
             {"type",
              {{"type", "string"},
               {"enum", {"float", "string"}},
               {"description", "Parameter type"}}}}},
           {"required", {"model_id", "node_name", "parameter_name", "type"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("model_id") || !params.contains("node_name") ||
                  !params.contains("parameter_name") || !params.contains("type"))
              {
                  return {{"error", "Missing required parameters"}};
              }

              uint32_t modelId = params["model_id"];
              std::string nodeName = params["node_name"];
              std::string parameterName = params["parameter_name"];
              std::string type = params["type"];

              try
              {
                  if (type == "float")
                  {
                      float value =
                        m_application->getFloatParameter(modelId, nodeName, parameterName);
                      return {{"success", true},
                              {"model_id", modelId},
                              {"node_name", nodeName},
                              {"parameter_name", parameterName},
                              {"type", type},
                              {"value", value}};
                  }
                  else if (type == "string")
                  {
                      std::string value =
                        m_application->getStringParameter(modelId, nodeName, parameterName);
                      return {{"success", true},
                              {"model_id", modelId},
                              {"node_name", nodeName},
                              {"parameter_name", parameterName},
                              {"type", type},
                              {"value", value}};
                  }
                  else
                  {
                      return {{"error", "Invalid parameter type: " + type}};
                  }
              }
              catch (const std::exception & e)
              {
                  return {{"error", "Failed to get parameter: " + std::string(e.what())}};
              }
          });

        // Phase 1D: Analysis tools (placeholder implementations)
        registerTool(
          "analyze_geometry",
          "Analyze geometry properties of the current document",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              if (!m_application->hasActiveDocument())
              {
                  return {{"error", "No active document"}};
              }

              // TODO: Implement actual geometry analysis
              return {
                {"success", true},
                {"analysis",
                 {{"message", "Geometry analysis not yet implemented"}, {"has_document", true}}}};
          });

        registerTool(
          "get_scene_hierarchy",
          "Get the scene hierarchy of the current document",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              if (!m_application->hasActiveDocument())
              {
                  return {{"error", "No active document"}};
              }

              // TODO: Implement actual scene hierarchy retrieval
              return {
                {"success", true},
                {"hierarchy",
                 {{"message", "Scene hierarchy not yet implemented"}, {"has_document", true}}}};
          });

        // Diagnostic tool for document loading verification
        registerTool(
          "get_document_info",
          "Get detailed information about the current document state",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              json info = {{"has_document", m_application->hasActiveDocument()},
                           {"document_path", m_application->getActiveDocumentPath()},
                           {"timestamp",
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count()}};

              if (m_application->hasActiveDocument())
              {
                  std::string path = m_application->getActiveDocumentPath();
                  info["path_length"] = path.length();
                  info["path_empty"] = path.empty();
                  info["has_valid_path"] = !path.empty();

                  // Add file existence check if path is not empty
                  if (!path.empty())
                  {
                      try
                      {
                          std::filesystem::path filePath(path);
                          info["file_exists"] = std::filesystem::exists(filePath);
                          info["file_size"] = std::filesystem::exists(filePath)
                                                ? std::filesystem::file_size(filePath)
                                                : 0;
                          info["file_extension"] = filePath.extension().string();
                      }
                      catch (const std::exception & e)
                      {
                          info["file_check_error"] = e.what();
                      }
                  }
              }

              return info;
          });

        // Phase 2A: Expression-based function creation
        registerTool(
          "create_function_from_expression",
          "Create a new function from a mathematical expression (supports TPMS, lattices, "
          "primitives, and custom expressions)",
          {{"type", "object"},
           {"properties",
            {{"name", {{"type", "string"}, {"description", "Function name"}}},
             {"expression",
              {{"type", "string"},
               {"description", "Mathematical expression (e.g., 'sin(x*2)*cos(y*3)')"}}},
             {"arguments",
              {{"type", "array"},
               {"items",
                {{"type", "object"},
                 {"properties",
                  {{"name", {{"type", "string"}}},
                   {"type", {{"type", "string"}, {"enum", {"float", "vec3"}}}},
                   {"default_value", {{"type", "number"}}}}},
                 {"required", {"name", "type"}}}},
               {"description", "Function input arguments"}}},
             {"output_type",
              {{"type", "string"},
               {"enum", {"float", "vec3"}},
               {"description", "Output type (default: float)"}}}}},
           {"required", {"name", "expression"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("name") || !params.contains("expression"))
              {
                  return {{"error", "Missing required parameters: name, expression"}};
              }

              std::string name = params["name"];
              std::string expression = params["expression"];
              std::string outputType = params.value("output_type", "float");

              try
              {
                  bool success =
                    m_application->createFunctionFromExpression(name, expression, outputType);
                  return {
                    {"success", success},
                    {"function_name", name},
                    {"expression", expression},
                    {"output_type", outputType},
                    {"message",
                     success ? "Function created successfully" : "Failed to create function"}};
              }
              catch (const std::exception & e)
              {
                  return {{"error", "Failed to create function: " + std::string(e.what())}};
              }
          });
    }

    void MCPServer::runStdioLoop()
    {
        std::string line;
        while (m_running && std::getline(std::cin, line))
        {
            if (!line.empty())
            {
                handleStdioMessage(line);
            }
        }
    }

    void MCPServer::handleStdioMessage(const std::string & line)
    {
        try
        {
            json request = json::parse(line);
            json response = processJSONRPCRequest(request);
            sendStdioResponse(response);
        }
        catch (const json::parse_error & e)
        {
            json errorResponse =
              createErrorResponse(0, -32700, "Parse error: " + std::string(e.what()));
            sendStdioResponse(errorResponse);
        }
        catch (const std::exception & e)
        {
            json errorResponse =
              createErrorResponse(0, -32603, "Internal error: " + std::string(e.what()));
            sendStdioResponse(errorResponse);
        }
    }

    void MCPServer::sendStdioResponse(const nlohmann::json & response)
    {
        std::cout << response.dump() << std::endl;
        std::cout.flush();
    }

    nlohmann::json MCPServer::processJSONRPCRequest(const nlohmann::json & request)
    {
        // Check for required fields
        if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0")
        {
            return createErrorResponse(
              json(nullptr), -32600, "Invalid Request - missing or invalid jsonrpc");
        }

        if (!request.contains("method"))
        {
            return createErrorResponse(json(nullptr), -32600, "Invalid Request - missing method");
        }

        std::string method = request["method"];
        auto id = request.contains("id") ? request["id"] : json(nullptr);

        if (method == "initialize")
        {
            return handleInitialize(request);
        }
        else if (method == "tools/list")
        {
            return handleListTools(request);
        }
        else if (method == "tools/call")
        {
            return handleCallTool(request);
        }
        else
        {
            return createErrorResponse(id, -32601, "Method not found: " + method);
        }
    }

} // namespace gladius::mcp
