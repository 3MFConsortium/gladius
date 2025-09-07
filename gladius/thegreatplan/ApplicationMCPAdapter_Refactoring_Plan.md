# ApplicationMCPAdapter Refactoring Plan

## Overview

The `ApplicationMCPAdapter.cpp` file has grown to over 3,800 lines of code, making it difficult to maintain, test, and extend. This plan outlines a comprehensive refactoring strategy to decompose the monolithic adapter into a collection of focused, single-responsibility classes organized around MCP tool categories.

## Current State Analysis

### Problems Identified
1. **Single Responsibility Violation**: The class handles 50+ methods across multiple domains
2. **High Complexity**: Methods range from simple getters to complex validation logic
3. **Poor Testability**: Monolithic structure makes unit testing difficult
4. **Code Duplication**: Similar error handling and validation patterns repeated
5. **Maintenance Burden**: Changes in one area can affect unrelated functionality

### Method Categories Analysis
Based on the interface analysis, methods fall into these logical groups:

1. **Application Lifecycle** (8 methods)
   - `getVersion`, `isRunning`, `getApplicationName`, `getStatus`
   - `setHeadlessMode`, `isHeadlessMode`, `showUI`, `isUIRunning`

2. **Document Management** (8 methods)
   - `hasActiveDocument`, `getActiveDocumentPath`, `createNewDocument`
   - `openDocument`, `saveDocument`, `saveDocumentAs`, `exportDocument`
   - `validateDocumentFor3MF`

3. **Function & Expression Operations** (7 methods)
   - `createFunctionFromExpression`, `createSDFFunction`, `createCSGOperation`
   - `applyTransformToFunction`, `analyzeFunctionProperties`
   - `generateMeshFromFunction`, `listAvailableFunctions`

4. **Node Graph Operations** (8 methods)
   - `getFunctionGraph`, `getNodeInfo`, `createNode`, `deleteNode`
   - `setParameterValue`, `createLink`, `deleteLink`
   - `setFloatParameter`, `getFloatParameter`, `setStringParameter`, `getStringParameter`

5. **3MF Resource Management** (8 methods)
   - `get3MFStructure`, `exportDocumentAs3MF`, `createLevelSet`
   - `createImage3DFunction`, `createVolumetricColor`, `createVolumetricProperty`
   - `setBuildItemObjectByIndex`, `setBuildItemTransformByIndex`, `modifyLevelSet`

6. **Scene & Hierarchy** (3 methods)
   - `getSceneHierarchy`, `getDocumentInfo`, `getModelBoundingBox`

7. **Rendering Operations** (4 methods)
   - `renderToFile`, `renderWithCamera`, `generateThumbnail`, `getOptimalCameraPosition`

8. **Validation & Analysis** (3 methods)
   - `validateForManufacturing`, `validateModel`, `performAutoValidation`

9. **Utility Operations** (3 methods)
   - `removeUnusedResources`, `executeBatchOperations`, `getLastErrorMessage`

## Proposed Architecture

### Base Classes

#### 1. `MCPToolBase` (Abstract Base Class)
```cpp
namespace gladius::mcp::tools
{
    class MCPToolBase
    {
    protected:
        Application* m_application;
        mutable std::string m_lastErrorMessage;
        
        // Common validation helpers
        bool validateApplication() const;
        bool validateActiveDocument() const;
        void setErrorMessage(const std::string& message) const;
        
    public:
        explicit MCPToolBase(Application* app);
        virtual ~MCPToolBase() = default;
        
        std::string getLastErrorMessage() const { return m_lastErrorMessage; }
    };
}
```

#### 2. `AsyncMCPToolBase` (For async operations)
```cpp
namespace gladius::mcp::tools
{
    class AsyncMCPToolBase : public MCPToolBase
    {
    protected:
        std::unique_ptr<mcp::CoroMCPAdapter> m_coroAdapter;
        
    public:
        explicit AsyncMCPToolBase(Application* app);
        virtual ~AsyncMCPToolBase() = default;
    };
}
```

### Tool Classes

#### 1. `ApplicationLifecycleTool`
**Responsibility**: Application state and configuration management
**Methods**: `getVersion`, `isRunning`, `getApplicationName`, `getStatus`, `setHeadlessMode`, `isHeadlessMode`, `showUI`, `isUIRunning`

#### 2. `DocumentManagementTool` (extends `AsyncMCPToolBase`)
**Responsibility**: Document lifecycle operations
**Methods**: `hasActiveDocument`, `getActiveDocumentPath`, `createNewDocument`, `openDocument`, `saveDocument`, `saveDocumentAs`, `exportDocument`, `validateDocumentFor3MF`

#### 3. `FunctionOperationsTool`
**Responsibility**: Function creation and manipulation
**Methods**: `createFunctionFromExpression`, `createSDFFunction`, `createCSGOperation`, `applyTransformToFunction`, `analyzeFunctionProperties`, `generateMeshFromFunction`, `listAvailableFunctions`

#### 4. `NodeGraphTool`
**Responsibility**: Node graph manipulation and parameter management
**Methods**: `getFunctionGraph`, `getNodeInfo`, `createNode`, `deleteNode`, `setParameterValue`, `createLink`, `deleteLink`, `setFloatParameter`, `getFloatParameter`, `setStringParameter`, `getStringParameter`

#### 5. `ResourceManagementTool`
**Responsibility**: 3MF resource creation and management
**Methods**: `get3MFStructure`, `exportDocumentAs3MF`, `createLevelSet`, `createImage3DFunction`, `createVolumetricColor`, `createVolumetricProperty`, `setBuildItemObjectByIndex`, `setBuildItemTransformByIndex`, `modifyLevelSet`

#### 6. `SceneHierarchyTool`
**Responsibility**: Scene inspection and hierarchy queries
**Methods**: `getSceneHierarchy`, `getDocumentInfo`, `getModelBoundingBox`

#### 7. `RenderingTool`
**Responsibility**: Rendering and visualization operations
**Methods**: `renderToFile`, `renderWithCamera`, `generateThumbnail`, `getOptimalCameraPosition`

#### 8. `ValidationTool`
**Responsibility**: Model validation and analysis
**Methods**: `validateForManufacturing`, `validateModel`, `performAutoValidation`

#### 9. `UtilityTool`
**Responsibility**: Maintenance and batch operations
**Methods**: `removeUnusedResources`, `executeBatchOperations`

### Refactored ApplicationMCPAdapter

The main adapter becomes a lightweight coordinator:

```cpp
class ApplicationMCPAdapter : public MCPApplicationInterface
{
private:
    Application* m_application;
    
    // Tool instances
    std::unique_ptr<tools::ApplicationLifecycleTool> m_lifecycleTool;
    std::unique_ptr<tools::DocumentManagementTool> m_documentTool;
    std::unique_ptr<tools::FunctionOperationsTool> m_functionTool;
    std::unique_ptr<tools::NodeGraphTool> m_nodeGraphTool;
    std::unique_ptr<tools::ResourceManagementTool> m_resourceTool;
    std::unique_ptr<tools::SceneHierarchyTool> m_sceneTool;
    std::unique_ptr<tools::RenderingTool> m_renderingTool;
    std::unique_ptr<tools::ValidationTool> m_validationTool;
    std::unique_ptr<tools::UtilityTool> m_utilityTool;

public:
    explicit ApplicationMCPAdapter(Application* app);
    ~ApplicationMCPAdapter() override = default;
    
    // All interface methods delegate to appropriate tools
    std::string getVersion() const override 
    { 
        return m_lifecycleTool->getVersion(); 
    }
    
    // ... other delegating methods
};
```

## Directory Structure

```
gladius/src/mcp/
├── ApplicationMCPAdapter.h
├── ApplicationMCPAdapter.cpp                    # Lightweight coordinator
├── MCPApplicationInterface.h                    # Unchanged
├── tools/
│   ├── MCPToolBase.h                           # Base class
│   ├── MCPToolBase.cpp
│   ├── AsyncMCPToolBase.h                      # Async base class
│   ├── AsyncMCPToolBase.cpp
│   ├── ApplicationLifecycleTool.h
│   ├── ApplicationLifecycleTool.cpp
│   ├── DocumentManagementTool.h
│   ├── DocumentManagementTool.cpp
│   ├── FunctionOperationsTool.h
│   ├── FunctionOperationsTool.cpp
│   ├── NodeGraphTool.h
│   ├── NodeGraphTool.cpp
│   ├── ResourceManagementTool.h
│   ├── ResourceManagementTool.cpp
│   ├── SceneHierarchyTool.h
│   ├── SceneHierarchyTool.cpp
│   ├── RenderingTool.h
│   ├── RenderingTool.cpp
│   ├── ValidationTool.h
│   ├── ValidationTool.cpp
│   ├── UtilityTool.h
│   └── UtilityTool.cpp
└── nodes/                                      # Existing node-related code
    └── ...
```

## Implementation Strategy

### Phase 1: Foundation (Week 1)
1. **Create base classes**
   - Implement `MCPToolBase` with common functionality
   - Implement `AsyncMCPToolBase` for async operations
   - Extract common validation and error handling patterns

2. **Setup build infrastructure**
   - Update CMakeLists.txt to include new tool directory
   - Ensure proper dependency management

### Phase 2: Tool Implementation (Weeks 2-4)
Implement tools in order of complexity and dependencies:

1. **Week 2**: Simple tools first
   - `ApplicationLifecycleTool` (simplest, no dependencies)
   - `SceneHierarchyTool` (read-only operations)
   - `UtilityTool` (independent operations)

2. **Week 3**: Medium complexity tools
   - `DocumentManagementTool` (core functionality, many dependencies)
   - `ValidationTool` (depends on document operations)
   - `RenderingTool` (depends on document state)

3. **Week 4**: Complex tools
   - `FunctionOperationsTool` (complex expression handling)
   - `ResourceManagementTool` (complex 3MF operations)
   - `NodeGraphTool` (complex graph manipulation)

### Phase 3: Integration (Week 5)
1. **Refactor ApplicationMCPAdapter**
   - Replace method implementations with tool delegation
   - Ensure all interface contracts are maintained
   - Implement proper tool lifecycle management

2. **Testing**
   - Unit tests for each tool class
   - Integration tests for the refactored adapter
   - Regression testing against existing functionality

### Phase 4: Optimization (Week 6)
1. **Performance optimization**
   - Lazy initialization of tools if beneficial
   - Memory usage optimization
   - Remove any performance regressions

2. **Documentation**
   - Update class documentation
   - Create tool-specific usage examples
   - Document the new architecture

## Benefits of This Approach

### 1. **Single Responsibility**
Each tool class has a clear, focused responsibility, making the codebase easier to understand and maintain.

### 2. **Enhanced Testability**
- Individual tools can be unit tested in isolation
- Mock dependencies can be easily injected
- Test coverage can be improved significantly

### 3. **Improved Maintainability**
- Changes to one tool don't affect others
- New functionality can be added to specific tools
- Bug fixes are localized to relevant tools

### 4. **Better Code Reuse**
- Common patterns extracted to base classes
- Shared validation and error handling logic
- Reduced code duplication

### 5. **Future Extensibility**
- New tools can be added easily
- Tool-specific optimizations possible
- Plugin-like architecture for future MCP extensions

## Risk Mitigation

### 1. **Backward Compatibility**
- All public interfaces remain unchanged
- Internal refactoring invisible to clients
- Comprehensive regression testing

### 2. **Performance**
- Measure baseline performance before refactoring
- Monitor for performance regressions
- Optimize delegation overhead if necessary

### 3. **Complexity Management**
- Gradual implementation reduces risk
- Maintain working state at each phase
- Rollback strategy for each phase

## Dependencies and Libraries

### Required Components
- **nlohmann/json**: Already used, no changes needed
- **Standard Library**: `<memory>`, `<string>`, `<vector>`, `<array>`
- **Existing Gladius Components**: Application, Document, Node system

### No Additional External Dependencies
This refactoring uses only existing dependencies and standard C++ features, maintaining the current dependency footprint.

## Conclusion

This refactoring plan transforms the monolithic `ApplicationMCPAdapter` into a well-structured, maintainable, and testable architecture. The tool-based approach provides clear separation of concerns while maintaining all existing functionality. The phased implementation strategy minimizes risk while delivering incremental value.

The resulting architecture will be more robust, easier to extend, and significantly more maintainable than the current monolithic approach.
