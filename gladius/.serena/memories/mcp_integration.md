# MCP Integration Architecture

## Overview
Gladius includes a comprehensive Model Context Protocol (MCP) server implementation that enables AI agent control and automation of the application.

## Architecture Components

### 1. MCPApplicationInterface (Header-only)
**File**: `src/mcp/MCPApplicationInterface.h`
- Clean, minimal interface for MCP integration
- No heavy dependencies to avoid build conflicts
- Essential methods: `getVersion()`, `isRunning()`, `getStatus()`

### 2. MCPServer (Core Implementation)
**Files**: `src/mcp/MCPServer.{h,cpp}`
- Complete HTTP server with JSON-RPC 2.0 protocol
- Built on cpp-httplib for HTTP handling
- Dual constructor pattern: accepts both shared_ptr and raw pointer
- CORS support for web-based AI agents
- Health endpoints for monitoring

### 3. ApplicationMCPAdapter (Bridge)
**Files**: `src/mcp/ApplicationMCPAdapter.{h,cpp}`
- Adapter pattern bridging Application class to MCP interface
- Isolates Application dependencies from MCP server
- Uses raw pointer to avoid circular dependencies

## Integration Pattern

```cpp
// In Application.h
std::unique_ptr<mcp::MCPServer> m_mcpServer;
std::unique_ptr<mcp::ApplicationMCPAdapter> m_mcpAdapter;

// In Application::enableMCPServer()
m_mcpAdapter = std::make_unique<mcp::ApplicationMCPAdapter>(this);
m_mcpServer = std::make_unique<mcp::MCPServer>(m_mcpAdapter.get());
bool success = m_mcpServer->start(port);
```

## Available Tools

### Built-in Tools
1. **ping** - Connectivity test
2. **get_status** - Application state information
3. **list_tools** - Enumerate available tools
4. **test_computation** - Mathematical computation testing

### Tool Extension
New tools can be added by:
1. Implementing tool logic in ApplicationMCPAdapter
2. Registering tool in MCPServer constructor
3. Following JSON schema for tool definitions

## HTTP Endpoints

### MCP Protocol
- `POST /mcp` - JSON-RPC 2.0 endpoint
- `OPTIONS /mcp` - CORS preflight support

### Monitoring
- `GET /health` - Health check endpoint

## Usage Modes

### 1. HTTP Mode (Default)
```bash
./gladius --enable-mcp --port 8080
```
- Web-accessible for browser-based AI agents
- CORS enabled for cross-origin requests
- RESTful health endpoints

### 2. STDIO Mode
```bash
./gladius --mcp-stdio
```
- Direct stdin/stdout communication
- For embedded MCP client integration
- No HTTP overhead

## AI Agent Integration

### Claude Desktop Configuration
```json
{
  "mcpServers": {
    "gladius": {
      "command": "/path/to/gladius",
      "args": ["--mcp-stdio"],
      "env": {}
    }
  }
}
```

### VS Code MCP Extension
Configure in workspace `.vscode/mcp.json`:
```json
{
  "servers": {
    "gladius": {
      "command": "/path/to/gladius",
      "args": ["--mcp-stdio"],
      "type": "stdio"
    }
  }
}
```

## Testing and Compliance

### Protocol Compliance
- JSON-RPC 2.0 specification compliance
- Official MCP protocol version 2025-06-18
- Schema validation for requests/responses

### Testing Tools
```bash
# Manual testing
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# Automated compliance tests
./src/mcp_compliance_test
```

## Benefits for AI Agents

### Capabilities
- **Real-time application control**: Start/stop, status monitoring
- **3D model manipulation**: Import/export, processing operations
- **Computation access**: OpenCL-based 3D computations
- **File system integration**: 3MF file operations
- **Workflow automation**: Multi-step 3D processing pipelines

### Use Cases
- Automated 3D model processing workflows
- AI-driven 3D design assistance
- Batch processing of 3MF files
- Integration with AI coding assistants
- Automated testing and validation

## Security Considerations

### Current Implementation
- No authentication (development mode)
- CORS enabled for development
- Local-only binding recommended

### Production Enhancements (Future)
- Authentication tokens
- Rate limiting
- Secure HTTPS endpoints
- Access control for sensitive operations

## Future Extensibility

### Planned Tool Categories
1. **Document Operations**: Open, save, import, export 3MF files
2. **3D Manipulation**: Transform, boolean operations, mesh processing
3. **Rendering Control**: Camera, lighting, visualization settings
4. **Computation Tools**: Custom OpenCL kernel execution
5. **Workflow Management**: Save/restore application state

### Plugin Architecture
The MCP system is designed to support plugin-based tool registration for easy extension by third-party developers.