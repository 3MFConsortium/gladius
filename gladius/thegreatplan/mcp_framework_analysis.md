# MCP Framework Analysis and Recommendations

## Executive Summary

This document provides a detailed analysis of available frameworks for implementing Model Context Protocol (MCP) support in Gladius, with specific focus on C++ compatibility and VCPKG availability.

## Framework Evaluation Matrix

| Framework | Language | VCPKG | Complexity | Performance | Community | Recommendation |
|-----------|----------|-------|------------|-------------|-----------|----------------|
| cpp-httplib | C++ | ✅ | Low | High | Medium | **Primary Choice** |
| Oat++ | C++ | ✅ | Medium | Very High | High | Fallback |
| Beast (Boost) | C++ | ✅ | High | Very High | High | Advanced Use |
| WebSocket++ | C++ | ✅ | Medium | High | Medium | WebSocket Only |
| Custom JSON-RPC | C++ | N/A | Low | High | N/A | Minimal Deps |

## Detailed Framework Analysis

### 1. cpp-httplib (Primary Recommendation)

#### Installation
```bash
vcpkg install httplib
```

#### Pros
- **Simple Integration**: Single-header library, easy to include
- **VCPKG Available**: Stable package management
- **Low Complexity**: Minimal learning curve
- **Good Performance**: Efficient for medium loads
- **Thread Safe**: Built-in support for concurrent requests
- **HTTPS Support**: SSL/TLS encryption available
- **Existing Usage**: Already familiar patterns in C++ ecosystem

#### Cons
- **Limited Advanced Features**: No built-in WebSocket support
- **Documentation**: Could be more comprehensive
- **Scalability**: May not handle extreme loads as well as specialized servers

#### Code Example
```cpp
#include <httplib.h>
#include <nlohmann/json.hpp>

class MCPServer {
public:
    MCPServer() {
        // Set up JSON-RPC endpoint
        m_server.Post("/rpc", [this](const httplib::Request& req, httplib::Response& res) {
            handleJSONRPC(req, res);
        });
    }
    
    void start(int port = 8080) {
        m_server.listen("localhost", port);
    }
    
private:
    httplib::Server m_server;
    
    void handleJSONRPC(const httplib::Request& req, httplib::Response& res) {
        try {
            auto json = nlohmann::json::parse(req.body);
            auto response = processRequest(json);
            res.set_content(response.dump(), "application/json");
        } catch (const std::exception& e) {
            // Error handling
        }
    }
};
```

### 2. Oat++ (Fallback Choice)

#### Installation
```bash
vcpkg install oatpp
```

#### Pros
- **High Performance**: Optimized for speed and memory usage
- **Full Featured**: Complete web framework with many features
- **WebSocket Support**: Built-in WebSocket implementation
- **Active Development**: Regular updates and improvements
- **Good Documentation**: Comprehensive guides and examples
- **Production Ready**: Used in enterprise applications

#### Cons
- **Complexity**: Steeper learning curve
- **Overhead**: More features than needed for basic MCP
- **Dependencies**: Larger dependency footprint
- **API Changes**: Faster-moving API may require updates

#### Code Example
```cpp
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/network/Server.hpp"

class MCPController : public oatpp::web::server::api::ApiController {
public:
    MCPController(OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper))
        : oatpp::web::server::api::ApiController(objectMapper) {}
    
    ENDPOINT("POST", "/rpc", rpc, BODY_DTO(Object<RequestDto>, request)) {
        // Process MCP request
        auto response = processRequest(request);
        return createDtoResponse(Status::CODE_200, response);
    }
};
```

### 3. Boost.Beast (Advanced Option)

#### Installation
```bash
vcpkg install boost-beast
```

#### Pros
- **Extremely High Performance**: Optimized for maximum throughput
- **Full HTTP/WebSocket**: Complete protocol implementations
- **Boost Quality**: High-quality, well-tested code
- **Flexible**: Highly customizable
- **Asynchronous**: Full async/await support

#### Cons
- **High Complexity**: Significant learning curve
- **Verbose**: Requires more boilerplate code
- **Boost Dependency**: Large dependency on Boost ecosystem
- **Overkill**: Too complex for basic MCP needs

### 4. Custom JSON-RPC Implementation

#### Pros
- **Minimal Dependencies**: Only requires JSON library
- **Full Control**: Complete control over implementation
- **Lightweight**: Minimal overhead
- **Gladius Integration**: Perfect fit for existing architecture

#### Cons
- **Development Time**: More implementation work required
- **Testing**: Need comprehensive protocol testing
- **Maintenance**: Ongoing maintenance responsibility
- **Missing Features**: Need to implement all HTTP features manually

#### Implementation Strategy
```cpp
namespace gladius::mcp {
    class JSONRPCServer {
    public:
        JSONRPCServer(std::shared_ptr<Application> app);
        void registerTool(const std::string& name, ToolFunction func);
        void start(int port);
        
    private:
        std::map<std::string, ToolFunction> m_tools;
        std::shared_ptr<Application> m_application;
    };
}
```

## Recommended Architecture

### Primary Implementation: cpp-httplib + nlohmann/json

```cpp
// File: src/mcp/MCPServer.h
#pragma once

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <functional>

namespace gladius::mcp {
    using ToolFunction = std::function<nlohmann::json(const nlohmann::json&)>;
    
    class MCPServer {
    public:
        explicit MCPServer(std::shared_ptr<Application> app);
        ~MCPServer();
        
        void registerTool(const std::string& name, ToolFunction func);
        void start(int port = 8080);
        void stop();
        bool isRunning() const;
        
    private:
        std::shared_ptr<Application> m_application;
        std::unique_ptr<httplib::Server> m_server;
        std::map<std::string, ToolFunction> m_tools;
        std::atomic<bool> m_running{false};
        
        void setupRoutes();
        nlohmann::json handleListTools();
        nlohmann::json handleCallTool(const nlohmann::json& request);
        void handleJSONRPC(const httplib::Request& req, httplib::Response& res);
    };
}
```

### Tool Registry Implementation

```cpp
// File: src/mcp/ToolRegistry.h
#pragma once

#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

namespace gladius::mcp {
    struct ToolInfo {
        std::string name;
        std::string description;
        nlohmann::json schema;
    };
    
    class Tool {
    public:
        virtual ~Tool() = default;
        virtual ToolInfo getInfo() const = 0;
        virtual nlohmann::json execute(const nlohmann::json& params) = 0;
    };
    
    class ToolRegistry {
    public:
        void registerTool(std::unique_ptr<Tool> tool);
        std::vector<ToolInfo> listTools() const;
        nlohmann::json executeTool(const std::string& name, const nlohmann::json& params);
        
    private:
        std::map<std::string, std::unique_ptr<Tool>> m_tools;
    };
}
```

## Integration with Gladius Application

### Application Modifications

```cpp
// File: src/Application.h - Add MCP support
#pragma once

#include "ui/MainWindow.h"
#include "ConfigManager.h"
#include "mcp/MCPServer.h" // New include

namespace gladius {
    class Application {
    public:
        Application();
        Application(int argc, char** argv);
        Application(std::filesystem::path const& filename);
        
        // New MCP methods
        void enableMCPServer(int port = 8080);
        void disableMCPServer();
        bool isMCPServerEnabled() const;
        
        ConfigManager& getConfigManager() { return m_configManager; }
        ConfigManager const& getConfigManager() const { return m_configManager; }
        
    private:
        ConfigManager m_configManager;
        ui::MainWindow m_mainWindow;
        std::unique_ptr<mcp::MCPServer> m_mcpServer; // New member
        
        void setupMCPTools(); // New method
    };
}
```

### Command Line Integration

```cpp
// File: src/main.cpp - Add MCP command line option
#include "Application.h"
#include <filesystem>
#include <iostream>
#include <string>

void printUsage() {
    std::cout << "Usage: gladius [options] [file]\n";
    std::cout << "Options:\n";
    std::cout << "  --mcp-server [port]  Enable MCP server (default port: 8080)\n";
    std::cout << "  --help              Show this help message\n";
}

int main(int argc, char** argv) {
    bool enableMCP = false;
    int mcpPort = 8080;
    std::optional<std::filesystem::path> filename;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--mcp-server") {
            enableMCP = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                mcpPort = std::stoi(argv[++i]);
            }
        } else if (arg == "--help") {
            printUsage();
            return 0;
        } else if (!arg.starts_with("--")) {
            filename = std::filesystem::path(arg);
        }
    }
    
    // Set working directory
    std::filesystem::current_path(std::filesystem::path(argv[0]).parent_path());
    
    // Create application
    gladius::Application app;
    
    // Enable MCP server if requested
    if (enableMCP) {
        app.enableMCPServer(mcpPort);
        std::cout << "MCP server enabled on port " << mcpPort << std::endl;
    }
    
    // Open file if specified
    if (filename && std::filesystem::exists(*filename)) {
        // Open file logic here
    }
    
    return 0;
}
```

## Performance Considerations

### Threading Model
```cpp
namespace gladius::mcp {
    class ThreadSafeMCPServer {
    public:
        ThreadSafeMCPServer(std::shared_ptr<Application> app)
            : m_application(app)
            , m_server(std::make_unique<httplib::Server>())
        {
            // Configure thread pool
            m_server->set_thread_pool_count(4);
        }
        
    private:
        std::shared_ptr<Application> m_application;
        std::unique_ptr<httplib::Server> m_server;
        std::mutex m_applicationMutex; // For thread-safe access
    };
}
```

### Memory Management
- Use RAII principles for all resources
- Implement proper cleanup in destructors
- Use smart pointers for automatic memory management
- Monitor memory usage for large operations

### Error Handling Strategy
```cpp
namespace gladius::mcp {
    enum class ErrorCode {
        SUCCESS = 0,
        INVALID_REQUEST = -1,
        TOOL_NOT_FOUND = -2,
        INVALID_PARAMETERS = -3,
        EXECUTION_FAILED = -4,
        INTERNAL_ERROR = -5
    };
    
    struct MCPError {
        ErrorCode code;
        std::string message;
        nlohmann::json details;
    };
    
    nlohmann::json createErrorResponse(const MCPError& error) {
        return {
            {"error", {
                {"code", static_cast<int>(error.code)},
                {"message", error.message},
                {"details", error.details}
            }}
        };
    }
}
```

## Testing Strategy

### Unit Tests
```cpp
// File: tests/mcp/MCPServerTest.cpp
#include <gtest/gtest.h>
#include "mcp/MCPServer.h"

class MCPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_app = std::make_shared<gladius::Application>();
        m_server = std::make_unique<gladius::mcp::MCPServer>(m_app);
    }
    
    std::shared_ptr<gladius::Application> m_app;
    std::unique_ptr<gladius::mcp::MCPServer> m_server;
};

TEST_F(MCPServerTest, ServerStartsAndStops) {
    EXPECT_FALSE(m_server->isRunning());
    
    m_server->start(8081);
    EXPECT_TRUE(m_server->isRunning());
    
    m_server->stop();
    EXPECT_FALSE(m_server->isRunning());
}
```

### Integration Tests
```cpp
// File: tests/mcp/MCPIntegrationTest.cpp
#include <gtest/gtest.h>
#include <httplib.h>
#include "mcp/MCPServer.h"

class MCPIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_app = std::make_shared<gladius::Application>();
        m_server = std::make_unique<gladius::mcp::MCPServer>(m_app);
        m_server->start(8082);
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    void TearDown() override {
        m_server->stop();
    }
    
    std::shared_ptr<gladius::Application> m_app;
    std::unique_ptr<gladius::mcp::MCPServer> m_server;
};

TEST_F(MCPIntegrationTest, ListToolsReturnsValidResponse) {
    httplib::Client client("localhost", 8082);
    
    nlohmann::json request = {
        {"id", "test-1"},
        {"method", "list_tools"},
        {"params", {}}
    };
    
    auto response = client.Post("/rpc", request.dump(), "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(200, response->status);
    
    auto json_response = nlohmann::json::parse(response->body);
    EXPECT_TRUE(json_response.contains("result"));
}
```

## Deployment Considerations

### Build System Integration
```cmake
# CMakeLists.txt additions
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(gladius 
    PRIVATE 
    httplib::httplib
    nlohmann_json::nlohmann_json
)

# Optional: Enable MCP support flag
option(ENABLE_MCP_SERVER "Enable MCP server support" ON)
if(ENABLE_MCP_SERVER)
    target_compile_definitions(gladius PRIVATE GLADIUS_MCP_ENABLED)
endif()
```

### Configuration Management
```cpp
// File: src/mcp/MCPConfig.h
namespace gladius::mcp {
    struct MCPConfig {
        bool enabled = false;
        int port = 8080;
        std::string host = "localhost";
        bool requireAuth = false;
        std::string authToken;
        
        static MCPConfig fromJSON(const nlohmann::json& json);
        nlohmann::json toJSON() const;
    };
}
```

## Conclusion

The recommended approach using **cpp-httplib + nlohmann/json** provides the optimal balance of:

- **Simplicity**: Easy to implement and maintain
- **Performance**: Good performance for expected loads  
- **Integration**: Seamless integration with existing Gladius architecture
- **Dependencies**: Minimal, well-managed dependencies through VCPKG
- **Flexibility**: Room for future enhancements

This approach allows for rapid development while maintaining the quality and reliability standards expected in the Gladius project. The modular design ensures that the MCP server can be easily extended with additional tools and capabilities as requirements evolve.
