/**
 * @file MCPServer.cpp
 * @brief Implementation of the MCP server for Gladius
 */

#include "MCPServer.h"
#include "FunctionArgument.h"
#include "MCPApplicationInterface.h"
#include <cctype>
#include <chrono>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace gladius::mcp
{
    namespace
    {
        // Minimal Base64 encoder (RFC 4648)
        static const char b64_table[] =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string base64Encode(const std::vector<unsigned char> & data)
        {
            std::string out;
            out.reserve(((data.size() + 2) / 3) * 4);

            size_t i = 0;
            while (i + 2 < data.size())
            {
                unsigned int triple = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
                out.push_back(b64_table[(triple >> 18) & 0x3F]);
                out.push_back(b64_table[(triple >> 12) & 0x3F]);
                out.push_back(b64_table[(triple >> 6) & 0x3F]);
                out.push_back(b64_table[triple & 0x3F]);
                i += 3;
            }

            if (i < data.size())
            {
                unsigned int triple = data[i] << 16;
                bool twoBytes = false;
                if (i + 1 < data.size())
                {
                    triple |= (data[i + 1] << 8);
                    twoBytes = true;
                }

                out.push_back(b64_table[(triple >> 18) & 0x3F]);
                out.push_back(b64_table[(triple >> 12) & 0x3F]);
                if (twoBytes)
                {
                    out.push_back(b64_table[(triple >> 6) & 0x3F]);
                    out.push_back('=');
                }
                else
                {
                    out.push_back('=');
                    out.push_back('=');
                }
            }

            return out;
        }

        std::string guessMimeTypeFromPath(const std::string & path)
        {
            auto dot = path.find_last_of('.');
            if (dot == std::string::npos)
                return "image/png"; // default
            auto ext = path.substr(dot + 1);
            for (auto & c : ext)
                c = static_cast<char>(::tolower(c));
            if (ext == "png")
                return "image/png";
            if (ext == "jpg" || ext == "jpeg")
                return "image/jpeg";
            if (ext == "bmp")
                return "image/bmp";
            if (ext == "gif")
                return "image/gif";
            return "image/png";
        }

        std::vector<unsigned char> readFileBinary(const std::string & path)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return {};
            in.seekg(0, std::ios::end);
            std::streampos size = in.tellg();
            if (size <= 0)
                return {};
            std::vector<unsigned char> buffer(static_cast<size_t>(size));
            in.seekg(0, std::ios::beg);
            in.read(reinterpret_cast<char *>(buffer.data()), size);
            return buffer;
        }
    } // namespace

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
        // ===================================================================
        // MINIMAL 3MF VOLUMETRIC EXTENSION TOOL SET
        // Essential tools for authoring 3MF files with volumetric extension
        // ===================================================================

        // BASIC STATUS & CONNECTIVITY
        registerTool(
          "get_status",
          "Get the current status of Gladius application and project state",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              return {{"application", m_application->getApplicationName()},
                      {"version", m_application->getVersion()},
                      {"status", m_application->getStatus()},
                      {"is_running", m_application->isRunning()},
                      {"has_active_document", m_application->hasActiveDocument()},
                      {"headless", m_application->isHeadlessMode()},
                      {"ui_running", m_application->isUIRunning()},
                      {"active_document_path", m_application->getActiveDocumentPath()}};
          });

        // MODEL STRUCTURE INSPECTION
        registerTool(
          "get_3mf_structure",
          "Get a comprehensive listing of the current 3MF model structure (build items and "
          "resources)",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              if (!m_application)
              {
                  return {{"success", false}, {"error", "No application available"}};
              }
              auto result = m_application->get3MFStructure();
              return result;
          });

        // FUNCTION GRAPH INTROSPECTION
        registerTool(
          "get_function_graph",
          "Get the node graph of a function (model) as JSON for introspection/visualization",
          {{"type", "object"},
           {"properties",
            {{"function_id",
              {{"type", "integer"},
               {"description",
                "ModelResourceID of the function (model) to serialize (from "
                "get_3mf_structure)"}}}}},
           {"required", {"function_id"}}},
          [this](const json & params) -> json
          {
              if (!m_application)
              {
                  return {{"success", false}, {"error", "No application available"}};
              }
              if (!params.contains("function_id"))
              {
                  return {{"success", false}, {"error", "Missing required parameter: function_id"}};
              }
              uint32_t function_id = params["function_id"];
              return m_application->getFunctionGraph(function_id);
          });

        // DOCUMENT MANAGEMENT (3MF FILES)
        registerTool(
          "create_document",
          "Create a new 3MF document with volumetric extension support",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              bool success = m_application->createNewDocument();
              return {
                {"success", success},
                {"message", success ? "New 3MF document created" : "Failed to create document"}};
          });

        registerTool(
          "open_document",
          "Open a 3MF document from file",
          {{"type", "object"},
           {"properties",
            {{"path", {{"type", "string"}, {"description", "Path to the 3MF file to open"}}}}},
           {"required", {"path"}}},
          [this](const json & params) -> json
          {
              std::string path = params["path"];
              bool success = m_application->openDocument(path);
              return {{"success", success}, {"path", path}};
          });

        registerTool(
          "save_document_as",
          "Save the current document as a 3MF file",
          {{"type", "object"},
           {"properties",
            {{"path", {{"type", "string"}, {"description", "Path where to save the 3MF file"}}}}},
           {"required", {"path"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("path") || params["path"].is_null())
              {
                  return {{"error", "Missing required parameter: path"}};
              }
              std::string path = params["path"];
              bool success = m_application->saveDocumentAs(path);
              return {{"success", success},
                      {"path", path},
                      {"message", m_application->getLastErrorMessage()}};
          });

        registerTool(
          "save_document",
          "Save the current document to its existing file",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              bool success = m_application->saveDocument();
              json result = {{"success", success},
                             {"message", m_application->getLastErrorMessage()}};
              if (success)
              {
                  result["path"] = m_application->getActiveDocumentPath();
              }
              return result;
          });

        // VOLUMETRIC FUNCTIONS (Core of 3MF Volumetric Extension)
        {
            // Build schema explicitly to avoid brace mismatches
            json createFromExprSchema;
            createFromExprSchema["type"] = "object";
            json props;
            props["name"] = {{"type", "string"}, {"description", "Function name"}};
            {
                json expr;
                expr["type"] = "string";
                expr["description"] =
                  "Expression syntax (not GLSL):\n"
                  "- Variables: x, y, z (3D coordinates)\n"
                  "- Or use a vec3 argument and component access: pos.x, pos.y, pos.z\n"
                  "- Operators: +, -, *, /, ^ (power)\n"
                  "- Functions: sin(), cos(), tan(), sqrt(), abs(), exp(), log(), pow()\n"
                  "- Constants: pi, e\n"
                  "- Grouping: parentheses (x + y) * z\n\n"
                  "Notes: no comments, no semicolons, no vector literals, no GLSL built-ins like "
                  "length().\n\n"
                  "Examples:\n"
                  "- Gyroid: sin(x)*cos(y) + sin(y)*cos(z) + sin(z)*cos(x)\n"
                  "- Sphere (r=5): sqrt(x*x + y*y + z*z) - 5\n"
                  "- Scaled wave (period 10 mm): sin(x*2*pi/10)*cos(y*2*pi/10)";
                expr["examples"] =
                  json::array({"sin(x)*cos(y) + sin(y)*cos(z) + sin(z)*cos(x)",
                               "sqrt(x*x + y*y + z*z) - 5",
                               "sin(x*2*pi/10)*cos(y*2*pi/10)",
                               "max(sqrt(x*x+y*y+z*z) - 123, sin(2*pi*x/30)*cos(2*pi*y/30) + "
                               "sin(2*pi*y/30)*cos(2*pi*z/30) + sin(2*pi*z/30)*cos(2*pi*x/30))"});
                props["expression"] = expr;
            }
            json itemsProps;
            itemsProps["name"] = {{"type", "string"}, {"description", "Argument name"}};
            {
                json typeProp;
                typeProp["type"] = "string";
                typeProp["enum"] = json::array({"float", "vec3"});
                typeProp["description"] =
                  "Argument type. If you want to use component access (e.g., pos.x), pass a vec3 "
                  "argument and reference it by that name (e.g., name=pos, type=vec3). "
                  "Alternatively, you can rely on the implicit coordinate variables x, y, z.";
                itemsProps["type"] = typeProp;
            }
            json itemsObj;
            itemsObj["type"] = "object";
            itemsObj["properties"] = itemsProps;
            itemsObj["required"] = json::array({"name", "type"});

            json arguments;
            arguments["type"] = "array";
            arguments["items"] = itemsObj;
            arguments["description"] = "Function input arguments";

            props["arguments"] = arguments;

            createFromExprSchema["properties"] = props;
            createFromExprSchema["required"] = json::array({"name", "expression"});

            registerTool(
              "create_function_from_expression",
              "Create a volumetric function from a simple math expression (not GLSL). Supported: "
              "variables x,y,z or component access like pos.x/pos.y/pos.z (if you pass a vec3 "
              "argument), operators + - * / ^, functions sin cos tan sqrt abs exp log pow, "
              "constants "
              "pi and e, and parentheses. No comments (//, /* */), no semicolons, and no "
              "GLSL-specific "
              "constructs.",
              createFromExprSchema,
              [this](const json & params) -> json
              {
                  if (!params.contains("name") || !params.contains("expression"))
                  {
                      return {{"success", false}, {"error", "Missing required parameters"}};
                  }

                  std::string name = params["name"];
                  std::string expression = params["expression"];
                  std::string outputType = params.value("output_type", "float");

                  // Parse arguments if provided
                  std::vector<FunctionArgument> arguments;
                  if (params.contains("arguments") && params["arguments"].is_array())
                  {
                      for (const auto & argJson : params["arguments"])
                      {
                          std::string argName = argJson["name"];
                          std::string argType = argJson["type"];
                          ArgumentType type =
                            (argType == "float") ? ArgumentType::Scalar : ArgumentType::Vector;
                          arguments.emplace_back(argName, type);
                      }
                  }

                  auto result = m_application->createFunctionFromExpression(
                    name, expression, outputType, arguments, "");

                  if (result.first)
                  {
                      return {{"success", true},
                              {"function_name", name},
                              {"expression", expression},
                              {"output_type", outputType},
                              {"function_id", result.second}};
                  }
                  else
                  {
                      return {{"success", false}, {"error", m_application->getLastErrorMessage()}};
                  }
              });
        }

        // LEVEL SETS (Convert functions to 3D geometry for 3MF)
        registerTool(
          "create_levelset",
          "Create a level set from a volumetric function - converts function to 3D geometry for "
          "3MF",
          {{"type", "object"},
           {"properties",
            {{"function_id",
              {{"type", "integer"}, {"description", "Resource ID of the volumetric function"}}}}},
           {"required", {"function_id"}}},
          [this](const json & params) -> json
          {
              uint32_t function_id = params["function_id"];
              auto result = m_application->createLevelSet(function_id);

              if (result.first)
              {
                  return {{"success", true},
                          {"function_id", function_id},
                          {"resource_id", result.second}};
              }
              else
              {
                  return {{"success", false}, {"error", m_application->getLastErrorMessage()}};
              }
          });

        // IMAGE3D SUPPORT (For FunctionFromImage3D - 3MF Volumetric Extension requirement)
        registerTool(
          "create_image3d_function",
          "Create a function from 3D image data - supports FunctionFromImage3D in 3MF volumetric "
          "extension",
          {{"type", "object"},
           {"properties",
            {{"name", {{"type", "string"}, {"description", "Function name"}}},
             {"image_path", {{"type", "string"}, {"description", "Path to image stack directory"}}},
             {"value_scale",
              {{"type", "number"}, {"description", "Scaling factor for image values"}}},
             {"value_offset", {{"type", "number"}, {"description", "Offset for image values"}}}}},
           {"required", {"name", "image_path"}}},
          [this](const json & params) -> json
          {
              std::string name = params["name"];
              std::string image_path = params["image_path"];
              float scale = params.value("value_scale", 1.0f);
              float offset = params.value("value_offset", 0.0f);

              auto result = m_application->createImage3DFunction(name, image_path, scale, offset);

              if (result.first)
              {
                  return {{"success", true},
                          {"function_name", name},
                          {"image_path", image_path},
                          {"scale", scale},
                          {"offset", offset},
                          {"resource_id", result.second}};
              }
              else
              {
                  return {{"success", false}, {"error", m_application->getLastErrorMessage()}};
              }
          });

        // VOLUMETRIC DATA (Properties attached to functions - 3MF Volumetric Extension)
        registerTool(
          "create_volumetric_color",
          "Create volumetric color data from a function - part of 3MF volumetric extension",
          {{"type", "object"},
           {"properties",
            {{"function_id",
              {{"type", "integer"},
               {"description", "Resource ID of function that defines color distribution"}}},
             {"channel",
              {{"type", "string"},
               {"enum", {"red", "green", "blue", "color"}},
               {"description", "Color channel from function"}}}}},
           {"required", {"function_id", "channel"}}},
          [this](const json & params) -> json
          {
              uint32_t function_id = params["function_id"];
              std::string channel = params["channel"];

              auto result = m_application->createVolumetricColor(function_id, channel);

              if (result.first)
              {
                  return {{"success", true},
                          {"function_id", function_id},
                          {"channel", channel},
                          {"resource_id", result.second}};
              }
              else
              {
                  return {{"success", false}, {"error", m_application->getLastErrorMessage()}};
              }
          });

        registerTool(
          "create_volumetric_property",
          "Create custom volumetric property data from a function - part of 3MF volumetric "
          "extension",
          {{"type", "object"},
           {"properties",
            {{"property_name",
              {{"type", "string"}, {"description", "Name of the custom property"}}},
             {"function_id",
              {{"type", "integer"},
               {"description", "Resource ID of function that defines property distribution"}}},
             {"channel", {{"type", "string"}, {"description", "Channel from function to use"}}}}},
           {"required", {"property_name", "function_id", "channel"}}},
          [this](const json & params) -> json
          {
              std::string property_name = params["property_name"];
              uint32_t function_id = params["function_id"];
              std::string channel = params["channel"];

              auto result =
                m_application->createVolumetricProperty(property_name, function_id, channel);

              if (result.first)
              {
                  return {{"success", true},
                          {"property_name", property_name},
                          {"function_id", function_id},
                          {"channel", channel},
                          {"resource_id", result.second}};
              }
              else
              {
                  return {{"success", false}, {"error", m_application->getLastErrorMessage()}};
              }
          });

        // PARAMETER MANAGEMENT
        registerTool("set_parameter",
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
                         uint32_t model_id = params["model_id"];
                         std::string node_name = params["node_name"];
                         std::string parameter_name = params["parameter_name"];
                         std::string type = params["type"];
                         auto value = params["value"];

                         bool success = false;
                         if (type == "float")
                         {
                             float float_value = value;
                             success = m_application->setFloatParameter(
                               model_id, node_name, parameter_name, float_value);
                         }
                         else if (type == "string")
                         {
                             std::string string_value = value;
                             success = m_application->setStringParameter(
                               model_id, node_name, parameter_name, string_value);
                         }

                         if (success)
                         {
                             return {{"success", true},
                                     {"model_id", model_id},
                                     {"node_name", node_name},
                                     {"parameter_name", parameter_name},
                                     {"value", value},
                                     {"type", type}};
                         }
                         else
                         {
                             return {{"success", false},
                                     {"error", m_application->getLastErrorMessage()}};
                         }
                     });

        // MODEL VALIDATION (Two-phase: graph sync + OpenCL compile)
        registerTool("validate_model",
                     "Validate the model in two phases: 1) graphs/lib3mf update, 2) OpenCL "
                     "compile; returns diagnostics",
                     {{"type", "object"},
                      {"properties",
                       {{"compile",
                         {{"type", "boolean"},
                          {"description", "If true, run OpenCL compile phase (default true)"}}},
                        {"max_messages",
                         {{"type", "integer"},
                          {"description", "Max diagnostic messages to include (default 50)"}}}}},
                      {"required", nlohmann::json::array()}},
                     [this](const json & params) -> json
                     {
                         if (!m_application)
                         {
                             return {{"success", false}, {"error", "No application available"}};
                         }
                         return m_application->validateModel(params);
                     });

        // BUILD ITEM MODIFICATION
        registerTool(
          "set_build_item_object",
          "Modify an existing build item to reference a different object (by ModelResourceID)",
          {{"type", "object"},
           {"properties",
            {{"build_item_index",
              {{"type", "integer"}, {"description", "Zero-based index in build list"}}},
             {"object_id",
              {{"type", "integer"},
               {"description", "ModelResourceID of target object (mesh/components/levelset)"}}}}},
           {"required", {"build_item_index", "object_id"}}},
          [this](const json & params) -> json
          {
              uint32_t idx = params.value("build_item_index", 0);
              uint32_t objId = params.value("object_id", 0);
              bool ok = m_application->setBuildItemObjectByIndex(idx, objId);
              return {{"success", ok}, {"message", m_application->getLastErrorMessage()}};
          });

        registerTool("set_build_item_transform",
                     "Set the transform (4x3 row-major) of an existing build item by index",
                     {{"type", "object"},
                      {"properties",
                       {{"build_item_index",
                         {{"type", "integer"}, {"description", "Zero-based index in build list"}}},
                        {"transform",
                         {{"type", "array"},
                          {"minItems", 12},
                          {"maxItems", 12},
                          {"items", {{"type", "number"}}},
                          {"description", "4x3 matrix row-major: r0c0,r0c1,r0c2,r1c0,...,r3c2"}}}}},
                      {"required", {"build_item_index", "transform"}}},
                     [this](const json & params) -> json
                     {
                         uint32_t idx = params.value("build_item_index", 0);
                         std::array<float, 12> tr{};
                         auto arr = params["transform"];
                         for (size_t i = 0; i < 12 && i < arr.size(); ++i)
                         {
                             tr[i] = static_cast<float>(arr[i]);
                         }
                         bool ok = m_application->setBuildItemTransformByIndex(idx, tr);
                         return {{"success", ok},
                                 {"message", m_application->getLastErrorMessage()}};
                     });

        // LEVELSET MODIFICATION
        registerTool(
          "modify_levelset",
          "Modify a level set's referenced function and/or output channel",
          {{"type", "object"},
           {"properties",
            {{"levelset_id",
              {{"type", "integer"}, {"description", "ModelResourceID of the level set"}}},
             {"function_id",
              {{"type", "integer"}, {"description", "Optional function ModelResourceID"}}},
             {"channel", {{"type", "string"}, {"description", "Optional output channel name"}}}}},
           {"required", {"levelset_id"}}},
          [this](const json & params) -> json
          {
              uint32_t lsId = params.value("levelset_id", 0);
              std::optional<uint32_t> fnId;
              if (params.contains("function_id") && !params["function_id"].is_null())
              {
                  fnId = params["function_id"].get<uint32_t>();
              }
              std::optional<std::string> channel;
              if (params.contains("channel") && !params["channel"].is_null())
              {
                  channel = params["channel"].get<std::string>();
              }
              bool ok = m_application->modifyLevelSet(lsId, fnId, channel);
              return {{"success", ok}, {"message", m_application->getLastErrorMessage()}};
          });

        // ===================================================================
        // RENDERING TOOLS
        // Create high-quality renderings and exports of 3MF models
        // ===================================================================

        // BASIC RENDERING TO FILE
        registerTool(
          "render_to_file",
          "Render the current 3MF model to an image file with specified resolution and format",
          {{"type", "object"},
           {"properties",
            {{"output_path",
              {{"type", "string"}, {"description", "File path where to save the rendered image"}}},
             {"width",
              {{"type", "integer"}, {"description", "Image width in pixels"}, {"default", 1024}}},
             {"height",
              {{"type", "integer"}, {"description", "Image height in pixels"}, {"default", 1024}}},
             {"format",
              {{"type", "string"},
               {"enum", {"png", "jpg"}},
               {"description", "Output format"},
               {"default", "png"}}},
             {"quality",
              {{"type", "number"},
               {"minimum", 0.0},
               {"maximum", 1.0},
               {"description", "Quality setting for lossy formats"},
               {"default", 0.9}}}}},
           {"required", {"output_path"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("output_path"))
              {
                  return {{"success", false}, {"error", "Missing required parameter: output_path"}};
              }

              std::string outputPath = params["output_path"];
              uint32_t width = params.value("width", 1024);
              uint32_t height = params.value("height", 1024);
              std::string format = params.value("format", "png");
              float quality = params.value("quality", 0.9f);

              bool success =
                m_application->renderToFile(outputPath, width, height, format, quality);

              if (success)
              {
                  return {{"success", true},
                          {"output_path", outputPath},
                          {"width", width},
                          {"height", height},
                          {"format", format},
                          {"quality", quality}};
              }
              else
              {
                  return {{"success", false}, {"error", m_application->getLastErrorMessage()}};
              }
          });

        // ADVANCED RENDERING WITH CAMERA CONTROL
        registerTool(
          "render_with_camera",
          "Render with full camera and lighting control for high-quality output",
          {{"type", "object"},
           {"properties",
            {{"output_path",
              {{"type", "string"}, {"description", "File path where to save the rendered image"}}},
             {"camera_settings",
              {{"type", "object"},
               {"description", "Camera parameters"},
               {"properties",
                {{"eye_position",
                  {{"type", "array"},
                   {"items", {{"type", "number"}}},
                   {"minItems", 3},
                   {"maxItems", 3},
                   {"description", "Camera position [x, y, z]"}}},
                 {"target_position",
                  {{"type", "array"},
                   {"items", {{"type", "number"}}},
                   {"minItems", 3},
                   {"maxItems", 3},
                   {"description", "Look-at target [x, y, z]"}}},
                 {"up_vector",
                  {{"type", "array"},
                   {"items", {{"type", "number"}}},
                   {"minItems", 3},
                   {"maxItems", 3},
                   {"description", "Up direction [x, y, z]"},
                   {"default", {0, 0, 1}}}},
                 {"field_of_view",
                  {{"type", "number"},
                   {"minimum", 10.0},
                   {"maximum", 150.0},
                   {"description", "Field of view in degrees"},
                   {"default", 45.0}}}}},
               {"required", {"eye_position", "target_position"}}}},
             {"render_settings",
              {{"type", "object"},
               {"description", "Rendering parameters"},
               {"properties",
                {{"width",
                  {{"type", "integer"},
                   {"minimum", 64},
                   {"maximum", 8192},
                   {"description", "Image width in pixels"},
                   {"default", 1024}}},
                 {"height",
                  {{"type", "integer"},
                   {"minimum", 64},
                   {"maximum", 8192},
                   {"description", "Image height in pixels"},
                   {"default", 1024}}},
                 {"format",
                  {{"type", "string"},
                   {"enum", {"png", "jpg"}},
                   {"description", "Output format"},
                   {"default", "png"}}},
                 {"quality",
                  {{"type", "number"},
                   {"minimum", 0.0},
                   {"maximum", 1.0},
                   {"description", "Quality for lossy formats"},
                   {"default", 0.9}}},
                 {"background_color",
                  {{"type", "array"},
                   {"items", {{"type", "number"}, {"minimum", 0.0}, {"maximum", 1.0}}},
                   {"minItems", 4},
                   {"maxItems", 4},
                   {"description", "Background color [r, g, b, a]"},
                   {"default", {0.2, 0.2, 0.2, 1.0}}}},
                 {"enable_shadows",
                  {{"type", "boolean"}, {"description", "Enable shadows"}, {"default", true}}},
                 {"enable_lighting",
                  {{"type", "boolean"},
                   {"description", "Enable lighting"},
                   {"default", true}}}}}}}}},
           {"required", {"output_path", "camera_settings"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("output_path") || !params.contains("camera_settings"))
              {
                  return {{"success", false}, {"error", "Missing required parameters"}};
              }

              std::string outputPath = params["output_path"];
              json cameraSettings = params["camera_settings"];
              json renderSettings = params.value("render_settings", json::object());

              bool success =
                m_application->renderWithCamera(outputPath, cameraSettings, renderSettings);

              if (success)
              {
                  // Attempt to inline the rendered image as base64 for immediate preview
                  std::string mime = guessMimeTypeFromPath(outputPath);
                  auto bytes = readFileBinary(outputPath);
                  json result = {{"success", true},
                                 {"output_path", outputPath},
                                 {"camera_settings", cameraSettings},
                                 {"render_settings", renderSettings}};
                  if (!bytes.empty())
                  {
                      std::string b64 = base64Encode(bytes);
                      result["image_mime_type"] = mime;
                      result["image_base64"] = b64; // raw base64 without prefix
                      result["image_data_url"] = std::string("data:") + mime + ";base64," + b64;
                      result["image_bytes"] = static_cast<uint32_t>(bytes.size());
                  }
                  else
                  {
                      result["warning"] = "Rendered file could not be read for inlining";
                  }
                  return result;
              }
              else
              {
                  return {{"success", false}, {"error", m_application->getLastErrorMessage()}};
              }
          });

        // THUMBNAIL GENERATION
        registerTool(
          "generate_thumbnail",
          "Generate a thumbnail image of the current model for preview purposes",
          {{"type", "object"},
           {"properties",
            {{"output_path",
              {{"type", "string"}, {"description", "File path where to save the thumbnail"}}},
             {"size",
              {{"type", "integer"},
               {"minimum", 64},
               {"maximum", 1024},
               {"description", "Thumbnail size in pixels (square)"},
               {"default", 256}}}}},
           {"required", {"output_path"}}},
          [this](const json & params) -> json
          {
              if (!params.contains("output_path"))
              {
                  return {{"success", false}, {"error", "Missing required parameter: output_path"}};
              }

              std::string outputPath = params["output_path"];
              uint32_t size = params.value("size", 256);

              // Caveman log via application/global logger if available
              try
              {
                  if (m_application && m_application->isHeadlessMode())
                  {
                      // Best-effort: use active document logger for visibility
                      // We avoid introducing new dependencies here
                      auto info = m_application->getDocumentInfo();
                      (void) info;
                  }
              }
              catch (...)
              {
              }

              bool success = m_application->generateThumbnail(outputPath, size);

              if (success)
              {
                  try
                  {
                      // Soft log after success
                      (void) m_application->getLastErrorMessage();
                  }
                  catch (...)
                  {
                  }
                  // Attempt to inline the generated thumbnail as base64 for immediate preview
                  std::string mime = guessMimeTypeFromPath(outputPath);
                  auto bytes = readFileBinary(outputPath);
                  json result = {{"success", true}, {"output_path", outputPath}, {"size", size}};
                  if (!bytes.empty())
                  {
                      std::string b64 = base64Encode(bytes);
                      result["image_mime_type"] = mime;
                      result["image_base64"] = b64; // raw base64 without prefix
                      result["image_data_url"] = std::string("data:") + mime + ";base64," + b64;
                      result["image_bytes"] = static_cast<uint32_t>(bytes.size());
                  }
                  else
                  {
                      result["warning"] = "Thumbnail file could not be read for inlining";
                  }
                  return result;
              }
              else
              {
                  try
                  {
                      (void) m_application->getLastErrorMessage();
                  }
                  catch (...)
                  {
                  }
                  return {{"success", false}, {"error", m_application->getLastErrorMessage()}};
              }
          });

        // OPTIMAL CAMERA POSITION
        registerTool(
          "get_optimal_camera_position",
          "Get suggested camera settings for the best view of the current model",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              auto result = m_application->getOptimalCameraPosition();
              bool ok = !(result.contains("error") && !result["error"].is_null());
              json out = {{"success", ok}, {"camera_settings", result}};
              if (!ok)
              {
                  out["error"] = result["error"]; // surface error at top-level too
              }
              return out;
          });

        // MODEL BOUNDING BOX
        registerTool(
          "get_model_bounding_box",
          "Get the axis-aligned bounding box of the whole 3MF model; auto-updates if needed",
          {{"type", "object"}, {"properties", json::object()}, {"required", json::array()}},
          [this](const json & params) -> json
          {
              (void) params;
              if (!m_application)
              {
                  return {{"success", false}, {"error", "No application available"}};
              }
              return m_application->getModelBoundingBox();
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
