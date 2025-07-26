# Model Context Protocol (MCP) Server Implementation Plan for Gladius

## Executive Summary

This plan outlines the implementation of a Model Context Protocol (MCP) server for Gladius, enabling AI agents to control the application programmatically. The server will provide tools for opening 3MF files, modifying implicit models, and generating renderings through a standardized protocol interface.

## 1. Research Phase: Understanding MCP

### 1.1 What is Model Context Protocol (MCP)?

Model Context Protocol (MCP) is an open standard developed by Anthropic that enables AI assistants to securely connect to and interact with external data sources and tools. It provides a standardized way for AI systems to:
- Access and query data from various sources
- Execute tools and functions
- Maintain context across interactions
- Ensure secure, controlled access to resources

### 1.2 MCP Architecture Components

**Core Components:**
- **MCP Host**: The AI assistant or application that connects to MCP servers
- **MCP Server**: Provides resources and tools to the host
- **Resources**: Data sources (files, databases, APIs)
- **Tools**: Executable functions that can be called by the host
- **Prompts**: Reusable prompt templates

**Communication:**
- Bidirectional JSON-RPC protocol
- Transport layers: stdio, HTTP, WebSocket
- Secure authentication and authorization

### 1.3 MCP for Design Applications

**Best Practices for Design Applications:**
- Provide high-level semantic operations rather than low-level API calls
- Implement proper validation and error handling for geometric operations
- Support undo/redo operations through command patterns
- Provide preview/visualization capabilities
- Implement proper file format validation
- Support batch operations for efficiency
- Provide clear documentation of coordinate systems and units

## 2. Framework Research and Selection

### 2.1 Evaluated Frameworks

#### 2.1.1 TinyMCP
**Pros:**
- Lightweight implementation
- Simple integration
- Good for basic MCP functionality
- Minimal dependencies

**Cons:**
- Limited advanced features
- Smaller community
- May require more custom implementation

**VCPKG Availability:** Not available in VCPKG

#### 2.1.2 Oat++ (Oatpp)
**Pros:**
- High-performance web framework
- Built-in WebSocket support
- Good HTTP server capabilities
- Available in VCPKG
- Well-documented
- Active community

**Cons:**
- May be overkill for simple MCP implementation
- More complex than needed for basic use cases

**VCPKG Availability:** ✅ Available as `oatpp`

#### 2.1.3 Alternative Approaches

**cpp-httplib:**
- Lightweight HTTP/HTTPS server
- Available in VCPKG as `httplib`
- Simple integration
- Good for basic MCP over HTTP

**WebSocket++ (websocketpp):**
- Dedicated WebSocket implementation
- Available in VCPKG
- Good for real-time communication

**nlohmann/json:**
- Already used in Gladius for JSON handling
- Perfect for JSON-RPC protocol implementation

### 2.2 Recommended Framework Selection

**Primary Choice: Custom Implementation with cpp-httplib + nlohmann/json**

**Rationale:**
- Leverages existing JSON infrastructure in Gladius
- cpp-httplib provides simple HTTP server capabilities
- Minimal additional dependencies
- Easy integration with existing codebase
- Full control over MCP protocol implementation

**Fallback Choice: Oat++ for advanced features**

## 3. Architecture Design

### 3.1 High-Level Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   AI Agent      │────▶│   MCP Server    │────▶│   Gladius Core  │
│  (MCP Host)     │     │   (HTTP/JSON)   │     │   Application   │
└─────────────────┘     └─────────────────┘     └─────────────────┘
                               │
                               ▼
                        ┌─────────────────┐
                        │  Tool Registry  │
                        │  • File Ops     │
                        │  • Model Ops    │
                        │  • Render Ops   │
                        └─────────────────┘
```

### 3.2 Component Architecture

#### 3.2.1 MCP Server Core
```cpp
namespace gladius::mcp {
    class MCPServer {
    public:
        MCPServer(std::shared_ptr<Application> app);
        void start(int port = 8080);
        void stop();
        
    private:
        std::shared_ptr<Application> m_application;
        std::unique_ptr<httplib::Server> m_server;
        ToolRegistry m_toolRegistry;
    };
}
```

#### 3.2.2 Tool Registry System
```cpp
namespace gladius::mcp {
    class Tool {
    public:
        virtual ~Tool() = default;
        virtual std::string getName() const = 0;
        virtual std::string getDescription() const = 0;
        virtual nlohmann::json getSchema() const = 0;
        virtual nlohmann::json execute(const nlohmann::json& params) = 0;
    };
    
    class ToolRegistry {
    public:
        void registerTool(std::unique_ptr<Tool> tool);
        std::vector<std::string> listTools() const;
        nlohmann::json executeTool(const std::string& name, const nlohmann::json& params);
    };
}
```

### 3.3 Integration Points with Gladius

#### 3.3.1 Application Integration
- Extend `gladius::Application` class to include MCP server
- Add command-line option to enable MCP server mode
- Ensure thread-safe access to core functionality

#### 3.3.2 Document Operations
- Leverage existing `Document` class for file operations
- Use existing 3MF import/export infrastructure
- Integrate with `ComputeCore` for rendering operations

#### 3.3.3 Model Manipulation
- Use existing node system for model modifications
- Leverage `nodes::Assembly` for model management
- Integrate with expression parser for dynamic modifications

## 4. Tool Implementation Plan

### 4.1 File Operations Tools

#### 4.1.1 OpenFileT ool
```json
{
  "name": "open_file",
  "description": "Open a 3MF file in Gladius",
  "parameters": {
    "type": "object",
    "properties": {
      "file_path": {
        "type": "string",
        "description": "Path to the 3MF file to open"
      }
    },
    "required": ["file_path"]
  }
}
```

#### 4.1.2 SaveFileTool
```json
{
  "name": "save_file",
  "description": "Save current document to 3MF file",
  "parameters": {
    "type": "object",
    "properties": {
      "file_path": {
        "type": "string",
        "description": "Output path for the 3MF file"
      }
    },
    "required": ["file_path"]
  }
}
```

### 4.2 Model Inspection Tools

#### 4.2.1 ListObjectsTool
```json
{
  "name": "list_objects",
  "description": "List all objects in the current document",
  "parameters": {
    "type": "object",
    "properties": {}
  }
}
```

#### 4.2.2 GetObjectInfoTool
```json
{
  "name": "get_object_info",
  "description": "Get detailed information about a specific object",
  "parameters": {
    "type": "object",
    "properties": {
      "object_id": {
        "type": "string",
        "description": "ID of the object to inspect"
      }
    },
    "required": ["object_id"]
  }
}
```

### 4.3 Model Modification Tools

#### 4.3.1 AddImplicitFunctionTool
```json
{
  "name": "add_implicit_function",
  "description": "Add a new implicit function to the model",
  "parameters": {
    "type": "object",
    "properties": {
      "function_type": {
        "type": "string",
        "enum": ["sphere", "box", "cylinder", "torus", "expression"],
        "description": "Type of implicit function to add"
      },
      "parameters": {
        "type": "object",
        "description": "Function-specific parameters"
      },
      "name": {
        "type": "string",
        "description": "Name for the new function"
      }
    },
    "required": ["function_type", "parameters"]
  }
}
```

#### 4.3.2 ModifyImplicitFunctionTool
```json
{
  "name": "modify_implicit_function",
  "description": "Modify parameters of an existing implicit function",
  "parameters": {
    "type": "object",
    "properties": {
      "function_id": {
        "type": "string",
        "description": "ID of the function to modify"
      },
      "parameters": {
        "type": "object",
        "description": "New parameter values"
      }
    },
    "required": ["function_id", "parameters"]
  }
}
```

#### 4.3.3 CreateExpressionTool
```json
{
  "name": "create_expression",
  "description": "Create a new implicit function from mathematical expression",
  "parameters": {
    "type": "object",
    "properties": {
      "expression": {
        "type": "string",
        "description": "Mathematical expression for the implicit function"
      },
      "name": {
        "type": "string",
        "description": "Name for the new function"
      }
    },
    "required": ["expression"]
  }
}
```

### 4.4 Rendering Tools

#### 4.4.1 RenderImageTool
```json
{
  "name": "render_image",
  "description": "Generate a rendering of the current model",
  "parameters": {
    "type": "object",
    "properties": {
      "width": {
        "type": "integer",
        "default": 1920,
        "description": "Image width in pixels"
      },
      "height": {
        "type": "integer",
        "default": 1080,
        "description": "Image height in pixels"
      },
      "output_path": {
        "type": "string",
        "description": "Path to save the rendered image"
      },
      "camera_position": {
        "type": "object",
        "properties": {
          "x": {"type": "number"},
          "y": {"type": "number"},
          "z": {"type": "number"}
        },
        "description": "Camera position for rendering"
      }
    },
    "required": ["output_path"]
  }
}
```

#### 4.4.2 RenderSliceTool
```json
{
  "name": "render_slice",
  "description": "Generate a 2D slice rendering of the model",
  "parameters": {
    "type": "object",
    "properties": {
      "z_level": {
        "type": "number",
        "description": "Z-level for the slice"
      },
      "output_path": {
        "type": "string",
        "description": "Path to save the slice image"
      }
    },
    "required": ["z_level", "output_path"]
  }
}
```

## 5. Implementation Phases

### Phase 1: Foundation (Weeks 1-2)
**Goals:** Set up basic MCP server infrastructure

**Tasks:**
1. Add cpp-httplib to vcpkg dependencies
2. Create basic MCP server class structure
3. Implement JSON-RPC protocol handling
4. Create tool registry system
5. Add basic error handling and logging

**Deliverables:**
- Basic MCP server that can respond to discovery requests
- Tool registry framework
- Integration with existing Application class

### Phase 2: Core File Operations (Weeks 3-4)
**Goals:** Implement basic file handling tools

**Tasks:**
1. Implement OpenFileTool
2. Implement SaveFileTool
3. Implement ListObjectsTool
4. Add proper error handling for file operations
5. Create unit tests for file operations

**Deliverables:**
- Working file operation tools
- Basic model inspection capabilities
- Comprehensive error handling

### Phase 3: Model Modification Tools (Weeks 5-7)
**Goals:** Enable model manipulation through MCP

**Tasks:**
1. Implement AddImplicitFunctionTool
2. Implement ModifyImplicitFunctionTool
3. Implement CreateExpressionTool
4. Add parameter validation
5. Integrate with existing node system

**Deliverables:**
- Complete model modification capabilities
- Expression-based function creation
- Robust parameter validation

### Phase 4: Rendering Capabilities (Weeks 8-9)
**Goals:** Add rendering and visualization tools

**Tasks:**
1. Implement RenderImageTool
2. Implement RenderSliceTool
3. Add camera control capabilities
4. Optimize rendering performance
5. Add batch rendering support

**Deliverables:**
- Complete rendering tool suite
- High-quality image generation
- Performance-optimized rendering

### Phase 5: Advanced Features (Weeks 10-12)
**Goals:** Add advanced capabilities and polish

**Tasks:**
1. Implement batch operation tools
2. Add undo/redo support through MCP
3. Implement model analysis tools
4. Add comprehensive documentation
5. Performance optimization and testing

**Deliverables:**
- Advanced analysis tools
- Complete documentation
- Production-ready MCP server

## 6. Technical Considerations

### 6.1 Thread Safety
- Gladius UI runs on main thread
- MCP server will run on separate thread
- Need proper synchronization for shared resources
- Use message passing for complex operations

### 6.2 Error Handling
- Comprehensive error codes for different failure modes
- Proper validation of input parameters
- Graceful handling of file format errors
- Clear error messages for debugging

### 6.3 Performance
- Implement asynchronous operations for large models
- Use progress reporting for long-running operations
- Optimize memory usage for large datasets
- Consider caching strategies for frequently accessed data

### 6.4 Security
- Validate file paths to prevent directory traversal
- Implement proper authentication if needed
- Sanitize mathematical expressions
- Rate limiting for resource-intensive operations

## 7. Testing Strategy

### 7.1 Unit Tests
- Individual tool functionality
- JSON-RPC protocol handling
- Parameter validation
- Error handling scenarios

### 7.2 Integration Tests
- Full MCP workflow testing
- File operation validation
- Rendering pipeline testing
- Cross-platform compatibility

### 7.3 Performance Tests
- Large model handling
- Concurrent operation support
- Memory usage optimization
- Rendering performance benchmarks

## 8. Documentation Plan

### 8.1 API Documentation
- Complete tool reference
- Parameter specifications
- Error code documentation
- Usage examples

### 8.2 Integration Guide
- Setup instructions
- Configuration options
- Troubleshooting guide
- Best practices

### 8.3 Example Applications
- Basic file manipulation examples
- Advanced model creation workflows
- Batch processing examples
- AI agent integration patterns

## 9. Success Metrics

### 9.1 Functionality Metrics
- All core tools implemented and tested
- 95%+ test coverage
- Support for all major 3MF features
- Sub-second response time for basic operations

### 9.2 Quality Metrics
- Zero critical security vulnerabilities
- Comprehensive error handling
- Clear documentation for all features
- Successful integration with popular AI frameworks

### 9.3 Performance Metrics
- Handle models with 1M+ triangles
- Support concurrent requests
- Memory usage within 2x of baseline
- Rendering performance within 10% of native

## 10. Risk Assessment

### 10.1 Technical Risks
- **Threading complexity**: Mitigate with careful design and testing
- **Performance impact**: Monitor and optimize critical paths
- **Memory management**: Use RAII and smart pointers consistently

### 10.2 Integration Risks
- **Breaking existing functionality**: Comprehensive regression testing
- **Platform compatibility**: Test on all supported platforms
- **Dependency management**: Use VCPKG for consistent builds

### 10.3 Mitigation Strategies
- Incremental development with regular testing
- Feature flags for experimental functionality
- Comprehensive logging and monitoring
- Regular performance benchmarking

## Conclusion

This plan provides a comprehensive roadmap for implementing MCP support in Gladius. The phased approach ensures steady progress while maintaining code quality and system stability. The focus on using established frameworks and existing Gladius infrastructure minimizes risk while maximizing reusability.

The implementation will enable powerful AI-driven workflows for 3D model manipulation and generation, opening new possibilities for automated design and manufacturing processes.
