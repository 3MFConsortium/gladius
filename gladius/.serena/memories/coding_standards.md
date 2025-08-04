# Coding Standards and Conventions

## Code Style

### C++ Standards
- **Standard**: C++20
- **Naming Conventions**:
  - **Classes/Structs**: PascalCase (e.g., `Application`, `MCPServer`)
  - **Functions/Methods**: camelCase (e.g., `enableMCPServer`, `getCurrentDocument`)
  - **Variables**: camelCase (e.g., `configManager`, `mainWindow`)
  - **Member Variables**: Prefix with `m_` (e.g., `m_mcpServer`, `m_headlessMode`)
  - **Constants**: UPPER_SNAKE_CASE
  - **Namespaces**: lowercase (e.g., `gladius`, `mcp`)

### Code Formatting
- **Formatter**: clang-format with custom configuration
- **Brace Style**: Allman style (opening brace on new line)
- **Line Length**: 100 characters maximum
- **Indentation**: 4 spaces, no tabs
- **Pointer Alignment**: Middle (`char * ptr`)

### clang-format Configuration
```
BreakBeforeBraces: Allman
ColumnLimit: 100
IndentWidth: 4
PointerAlignment: Middle
UseTab: Never
AllowShortFunctionsOnASingleLine: None
AlwaysBreakTemplateDeclarations: true
```

## Documentation Standards

### Comments
- **Header Comments**: Doxygen-style for public APIs
- **Inline Comments**: Explain complex logic, not obvious code
- **TODO/FIXME**: Use standard tags for future work

### File Headers
Include license and copyright information where appropriate.

## Architecture Patterns

### Design Principles
- **RAII**: Resource Acquisition Is Initialization
- **Adapter Pattern**: Used for MCP integration (ApplicationMCPAdapter)
- **Dependency Injection**: Constructor injection preferred
- **Interface Segregation**: Minimal interfaces (MCPApplicationInterface)

### Memory Management
- **Smart Pointers**: Prefer `std::unique_ptr` and `std::shared_ptr`
- **Raw Pointers**: Only for non-owning references (adapter pattern)
- **RAII**: Automatic resource management

### Error Handling
- **Exceptions**: Used for error propagation
- **Return Values**: For expected failures
- **JSON-RPC**: Proper error codes for MCP protocol

## Build Standards

### Compiler Flags
- **Debug**: `/Z7` (MSVC), `-g` (GCC/Clang)
- **Release**: Optimized with debug info
- **Warnings**: Treat as errors where practical

### Dependencies
- **vcpkg**: All external dependencies managed through vcpkg
- **Version Pinning**: Specific versions in vcpkg.json
- **Build Cache**: buildcache when available for faster builds