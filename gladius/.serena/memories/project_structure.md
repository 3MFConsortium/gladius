# Gladius Project Structure

## Directory Layout

```
gladius/
├── src/                          # Main source code
│   ├── Application.{h,cpp}       # Main application class
│   ├── main.cpp                  # Entry point
│   ├── api/                      # API bindings (C++, C#, Python)
│   ├── compute/                  # OpenCL computation modules
│   ├── contour/                  # Contour extraction
│   ├── io/                       # File I/O operations
│   ├── mcp/                      # MCP server implementation
│   ├── nodes/                    # Node graph system
│   ├── threading/                # Threading utilities
│   ├── ui/                       # User interface
│   └── kernel/                   # OpenCL kernels (.cl files)
├── tests/
│   ├── unittests/               # Unit tests with GTest
│   └── integrationtests/        # Integration tests
├── components/                   # Third-party components
│   ├── fonts/                   # Font files
│   ├── IconFontCppHeaders/      # Icon font definitions
│   ├── licenses/                # License files
│   └── psimpl/                  # Polyline simplification
├── examples/                     # Example 3MF files
├── library/                      # Function libraries
├── documentation/               # Documentation and images
├── out/                         # Build output directory
├── vcpkg-overlay-ports/         # Custom vcpkg ports
├── vcpkg-triplets/              # Custom vcpkg triplets
└── cmake/                       # CMake modules
```

## Key Source Files

### Core Application
- `src/Application.{h,cpp}` - Main application class with UI and MCP integration
- `src/main.cpp` - Entry point and command line parsing
- `src/Document.{h,cpp}` - Document model for 3MF files
- `src/ConfigManager.{h,cpp}` - Application configuration

### Computation Engine
- `src/compute/` - OpenCL-based computation modules
- `src/kernel/*.cl` - OpenCL kernel implementations
- `src/ComputeContext.{h,cpp}` - OpenCL context management

### MCP Integration
- `src/mcp/MCPServer.{h,cpp}` - HTTP server with JSON-RPC protocol
- `src/mcp/MCPApplicationInterface.h` - Clean interface for MCP integration
- `src/mcp/ApplicationMCPAdapter.{h,cpp}` - Adapter connecting Application to MCP

### 3D Processing
- `src/MeshResource.{h,cpp}` - Mesh processing and management
- `src/VdbResource.{h,cpp}` - OpenVDB volumetric data
- `src/contour/` - Contour extraction from 3D models

### File I/O
- `src/io/` - Various file format readers/writers
- `src/CliReader.{h,cpp}` - CLI file format support
- `src/CliWriter.{h,cpp}` - CLI file export

### User Interface
- `src/ui/` - ImGui-based user interface components
- Node-based graphical programming interface

## Build Configuration

### CMake Structure
- `CMakeLists.txt` - Root CMake configuration
- `CMakePresets.json` - Build presets for different platforms
- `vcpkg.json` - Dependency specification

### Build Presets
- **Windows**: `x64-debug`, `x64-release`, `x64-release-debug`
- **Linux**: `linux-debug`, `linux-release`, `linux-releaseWithDebug`
- **Coverage**: Special builds with coverage analysis

## Dependencies (vcpkg.json)

### Core Libraries
- `fmt` - String formatting
- `eigen3` - Linear algebra
- `opencl` - Parallel computing
- `nlohmann-json` - JSON processing

### 3D Graphics
- `glad`, `glfw3` - OpenGL
- `imgui` - Immediate mode GUI
- `openmesh` - Mesh processing
- `openvdb` - Volumetric data

### File Formats
- `lib3mf` - 3MF file format
- `lodepng` - PNG images
- `pugixml` - XML processing
- `minizip` - ZIP compression

### Networking
- `cpp-httplib` - HTTP server for MCP

### Testing
- `gtest` - Unit testing framework