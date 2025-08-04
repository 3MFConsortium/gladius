# Task Completion Guidelines

## Code Development Workflow

### Before Starting Development
1. **Pull latest changes**: `git pull` to sync with remote
2. **Check build status**: Ensure project builds cleanly
3. **Run existing tests**: Verify baseline functionality
4. **Review task requirements**: Understand what needs to be implemented

### During Development
1. **Follow coding standards**: Use established naming conventions and formatting
2. **Write tests first**: TDD approach where applicable
3. **Incremental commits**: Small, focused commits with clear messages
4. **Documentation updates**: Update relevant documentation as you go

### Code Quality Checks
1. **Build verification**: 
   ```bash
   cmake --build out/build/linux-releaseWithDebug --parallel 8
   ```

2. **Run tests**:
   ```bash
   ctest --preset ReleaseWithDebug -V
   # Or specific test:
   ./out/build/linux-releaseWithDebug/tests/unittests/gladius_test
   ```

3. **Code formatting**:
   ```bash
   find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
   ```

4. **Static analysis** (if applicable):
   ```bash
   clang-tidy src/**/*.cpp -- -I. -Iout/build/linux-releaseWithDebug/vcpkg_installed/x64-linux/include
   ```

### MCP-Specific Tasks

For MCP server development:

1. **Test MCP functionality**:
   ```bash
   # Start server
   ./out/build/linux-releaseWithDebug/src/gladius --enable-mcp --port 8080
   
   # Test endpoints
   curl http://localhost:8080/health
   curl -X POST http://localhost:8080/mcp -H "Content-Type: application/json" \
     -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
   ```

2. **Run MCP compliance tests**:
   ```bash
   cd src/
   g++ -std=c++17 -I. -I../out/build/linux-releaseWithDebug/vcpkg_installed/x64-linux/include \
       mcp_compliance_test.cpp mcp/MCPServer.cpp -o mcp_compliance_test -lpthread
   ./mcp_compliance_test
   ```

### Final Checklist

Before considering a task complete:

#### ✅ Build and Test
- [ ] Project builds without errors or warnings
- [ ] All existing tests pass
- [ ] New functionality has tests (unit/integration)
- [ ] MCP server functionality tested (if applicable)

#### ✅ Code Quality
- [ ] Code follows established naming conventions
- [ ] clang-format applied to all modified files
- [ ] No obvious code smells or technical debt
- [ ] Proper error handling implemented

#### ✅ Documentation
- [ ] Code is adequately commented
- [ ] Public APIs have doxygen documentation
- [ ] README or relevant docs updated if needed
- [ ] MCP tools documented if new tools added

#### ✅ Version Control
- [ ] Changes committed with descriptive messages
- [ ] No sensitive information in commits
- [ ] Branch is ready for merge/review

#### ✅ Integration
- [ ] MCP server starts without errors
- [ ] Application runs in both GUI and headless modes
- [ ] OpenCL functionality works (if modified)
- [ ] File I/O operations work correctly

### Testing Strategy

#### Unit Tests
- Located in `tests/unittests/`
- Follow naming convention: `[Component]_tests.cpp`
- Use GTest framework
- Test individual functions and classes

#### Integration Tests
- Located in `tests/integrationtests/`
- Test component interactions
- Include file I/O and MCP protocol tests

#### Manual Testing
- Run application and verify UI functionality
- Test 3MF file import/export
- Verify MCP server with real clients
- Check OpenCL computation results

### Performance Considerations

For computationally intensive tasks:
1. **Profile performance**: Use Tracy profiler if enabled
2. **OpenCL optimization**: Verify GPU kernels perform well
3. **Memory usage**: Check for leaks with valgrind
4. **Build cache**: Ensure buildcache is working for faster builds

### Deployment Verification

Before release:
1. **Cross-platform testing**: Test on both Windows and Linux if possible
2. **Dependency verification**: Ensure vcpkg dependencies are correctly specified
3. **Package generation**: Test CPack installer generation
4. **MCP client compatibility**: Test with various MCP clients

This systematic approach ensures high-quality, maintainable code that integrates well with the existing Gladius ecosystem.