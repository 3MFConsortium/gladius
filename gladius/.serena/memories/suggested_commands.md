# Essential Development Commands

## Build Commands

### Primary Build (Recommended)
```bash
# Build everything using VS Code task or directly
cd /home/jan/projects/gladius/gladius
cmake --preset linux-releaseWithDebug -B out/build/linux-releaseWithDebug
cmake --build out/build/linux-releaseWithDebug --parallel 8
```

### Alternative Build Commands
```bash
# Debug build
cmake --preset linux-debug -B out/build/linux-debug
cmake --build out/build/linux-debug

# Release build
cmake --preset linux-release -B out/build/linux-release
cmake --build out/build/linux-release

# Build without OpenCL tests (for systems without OpenCL)
cmake --preset linux-releaseWithDebug-noOpenCL -B out/build/linux-releaseWithDebug-noOpenCL
cmake --build out/build/linux-releaseWithDebug-noOpenCL
```

## Testing Commands

### Run Tests
```bash
# Run all tests using CTest preset
cd /home/jan/projects/gladius/gladius
ctest --preset ReleaseWithDebug -V

# Run tests without OpenCL requirements
ctest --preset ReleaseWithDebug-noOpenCL -V

# Run specific test executable
./out/build/linux-releaseWithDebug/tests/unittests/gladius_test
```

### Coverage Analysis
```bash
# Configure with coverage (Clang)
CC=clang CXX=clang++ cmake --preset linux-releaseWithDebug -DENABLE_COVERAGE=ON -B out/build/linux-coverage-clang

# Configure with coverage (GCC)
CC=gcc CXX=g++ cmake --preset linux-releaseWithDebug -DENABLE_COVERAGE=ON -B out/build/linux-coverage-gcc

# Run coverage analysis
cmake --build out/build/linux-coverage-clang --target run-coverage
cmake --build out/build/linux-coverage-gcc --target run-coverage

# Open coverage reports
xdg-open out/build/linux-coverage-clang/coverage_html/index.html
xdg-open out/build/linux-coverage-gcc/coverage_html/index.html
```

## Code Quality Commands

### Formatting
```bash
# Format code with clang-format (configuration in .clang-format)
find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### Linting
```bash
# Run clang-tidy (configuration in .clang-tidy)
clang-tidy src/**/*.cpp -- -I. -Iout/build/linux-releaseWithDebug/vcpkg_installed/x64-linux/include
```

## Running Gladius

### Standard Execution
```bash
# Run built application
./out/build/linux-releaseWithDebug/src/gladius

# Run with MCP server enabled
./out/build/linux-releaseWithDebug/src/gladius --enable-mcp --port 8080

# Run in headless mode (no GUI)
./out/build/linux-releaseWithDebug/src/gladius --headless
```

## MCP Server Commands

### Start MCP Server
```bash
# HTTP mode (for web clients)
./out/build/linux-releaseWithDebug/src/gladius --enable-mcp --port 8080

# STDIO mode (for direct client integration)
./out/build/linux-releaseWithDebug/src/gladius --mcp-stdio
```

### Test MCP Server
```bash
# Health check
curl http://localhost:8080/health

# List tools
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# Get application status
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"get_status","arguments":{}}}'
```

## System Utilities (Linux)

### File Operations
```bash
# Find files
find . -name "*.cpp" -type f
find . -name "*.h" -type f

# Search in files
grep -r "pattern" src/
grep -r "TODO" src/ --include="*.cpp" --include="*.h"

# Directory listing
ls -la
tree -L 3  # if tree is installed
```

### Process Management
```bash
# Find running gladius processes
ps aux | grep gladius

# Kill process by name
pkill gladius

# Monitor system resources
htop  # if installed
top
```

### Development Tools
```bash
# Git operations
git status
git add .
git commit -m "message"
git push
git pull

# CMake cache cleanup
rm -rf out/build/linux-releaseWithDebug/CMakeCache.txt
rm -rf out/build/linux-releaseWithDebug/CMakeFiles/

# vcpkg operations (if needed)
~/vcpkg/vcpkg install --triplet x64-linux
~/vcpkg/vcpkg update
```

## Package Installation Commands

### Install Dependencies (Debian/Ubuntu)
```bash
# Development tools
sudo apt update
sudo apt install build-essential cmake ninja-build git curl zip unzip pkg-config

# OpenCL development
sudo apt install opencl-headers ocl-icd-opencl-dev

# OpenGL development
sudo apt install libgl1-mesa-dev libglu1-mesa-dev

# X11 development (for GLFW)
sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

## Task Completion Checklist

After completing any development task:

1. **Build**: Ensure project builds without errors
2. **Test**: Run relevant unit and integration tests
3. **Format**: Apply clang-format to modified files
4. **Lint**: Check with clang-tidy if applicable
5. **Documentation**: Update relevant documentation
6. **Commit**: Create meaningful git commit with descriptive message