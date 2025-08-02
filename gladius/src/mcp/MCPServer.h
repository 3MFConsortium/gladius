/**
 * @file MCPServer.h
 * @brief Model Context Protocol server implementation for Gladius
 */

#pragma once

#include <atomic>
#include <functional>
#include <httplib.h>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>

namespace gladius
{
    class MCPApplicationInterface;
    class Document;
}

namespace gladius::mcp
{
    /// Function signature for MCP tools
    using ToolFunction = std::function<nlohmann::json(const nlohmann::json &)>;

    /// Tool information structure
    struct ToolInfo
    {
        std::string name;
        std::string description;
        nlohmann::json schema;
    };

    /// Transport type for MCP server
    enum class TransportType
    {
        HTTP, ///< HTTP transport (for web clients)
        STDIO ///< Stdio transport (for VS Code and other tools)
    };

    /**
     * @brief MCP Server implementation supporting both HTTP and stdio transports
     *
     * Provides a Model Context Protocol server that can work with VS Code
     * (via stdio) or web clients (via HTTP).
     */
    class MCPServer
    {
      public:
        /**
         * @brief Constructor with shared pointer
         * @param app Shared pointer to application interface
         */
        explicit MCPServer(std::shared_ptr<MCPApplicationInterface> app);

        /**
         * @brief Constructor with raw pointer (for adapter pattern)
         * @param app Raw pointer to application interface
         */
        explicit MCPServer(
          MCPApplicationInterface * app); /**
                                           * @brief Destructor - ensures server is stopped
                                           */
        ~MCPServer();

        /**
         * @brief Register a tool with the server
         * @param name Tool name
         * @param description Tool description
         * @param schema JSON schema for tool parameters
         * @param func Tool function implementation
         */
        void registerTool(const std::string & name,
                          const std::string & description,
                          const nlohmann::json & schema,
                          ToolFunction func);

        /**
         * @brief Start the MCP server
         * @param port Port to listen on (for HTTP transport, default: 8080)
         * @param transport Transport type (HTTP or STDIO)
         * @return true if server started successfully
         */
        bool start(int port = 8080, TransportType transport = TransportType::HTTP);

        /**
         * @brief Start the MCP server with stdio transport
         * @return true if server started successfully
         */
        bool startStdio();

        /**
         * @brief Start the MCP server with HTTP transport
         * @param port Port to listen on
         * @return true if server started successfully
         */
        bool startHTTP(int port);

        /**
         * @brief Run the stdio message loop (blocking)
         */
        void runStdioLoop();

        /**
         * @brief Stop the MCP server
         */
        void stop();

        /**
         * @brief Check if server is running
         * @return true if server is running
         */
        bool isRunning() const;

        /**
         * @brief Get the port the server is listening on
         * @return Port number, or 0 if not running
         */
        int getPort() const;

        /**
         * @brief Get list of registered tools (for testing)
         * @return Vector of tool information
         */
        std::vector<ToolInfo> getRegisteredTools() const;

        /**
         * @brief Execute a tool directly (for testing)
         * @param toolName Name of the tool to execute
         * @param params Parameters for the tool
         * @return Tool execution result
         */
        nlohmann::json executeTool(const std::string & toolName, const nlohmann::json & params);

      private:
        MCPApplicationInterface *
          m_application; // Raw pointer (can work with shared_ptr or adapter)
        std::shared_ptr<MCPApplicationInterface> m_applicationShared; // Holds shared_ptr if used
        std::map<std::string, ToolInfo> m_toolInfo;
        std::map<std::string, ToolFunction> m_tools;
        std::atomic<bool> m_running{false};
        int m_port{0};
        TransportType m_transportType{TransportType::HTTP};

        // HTTP server components
        std::unique_ptr<httplib::Server> m_server;
        std::thread m_serverThread;

        // Stdio transport components
        std::thread m_stdioThread;

        /// Setup built-in tools
        void setupBuiltinTools();

        /// Setup HTTP routes
        void setupRoutes();

        /// Handle JSON-RPC requests
        void handleJSONRPC(const httplib::Request & req, httplib::Response & res);

        /// Handle MCP initialize request
        nlohmann::json handleInitialize(const nlohmann::json & request);

        /// Handle tools/list request
        nlohmann::json handleListTools(const nlohmann::json & request);

        /// Handle tools/call request
        nlohmann::json handleCallTool(const nlohmann::json & request);

        /// Create error response
        nlohmann::json createErrorResponse(int id, int code, const std::string & message) const;

        /// Handle stdio message processing
        void handleStdioMessage(const std::string & line);

        /// Send JSON response to stdout
        void sendStdioResponse(const nlohmann::json & response);

        /// Process JSON-RPC request and return response
        nlohmann::json processJSONRPCRequest(const nlohmann::json & request);
    };

} // namespace gladius::mcp
