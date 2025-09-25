# Phase 1 MCP Tools Implementation Review

## Overview

After analyzing the Gladius codebase, I've identified the existing methods and APIs that align perfectly with our Phase 1 MCP tools. Here's a detailed review showing how each tool can be implemented using existing Gladius functionality.

## Tool-by-Tool Implementation Analysis

### **1. Document Management Tools**

#### `create_document`
```json
{
  "name": "create_document",
  "description": "Create a new Gladius document",
  "inputSchema": {
    "type": "object",
    "properties": {
      "name": {"type": "string", "description": "Document name"},
      "template": {"type": "string", "enum": ["empty", "basic", "template"], "description": "Starting template"}
    },
    "required": ["name"]
  }
}
```

**✅ Implementation Strategy:**
```cpp
// Existing methods in Document class:
- Document::newModel()           // Create new model
- Document::newEmptyModel()      // Create empty model  
- Document::newFromTemplate()    // Load from template.3mf
```

**🔧 Suggested Implementation:**
```cpp
nlohmann::json handleCreateDocument(const nlohmann::json& params) {
    std::string name = params["name"];
    std::string template_type = params.value("template", "empty");
    
    if (template_type == "empty") {
        m_document->newEmptyModel();
    } else if (template_type == "template") {
        m_document->newFromTemplate();
    } else {
        m_document->newModel();
    }
    
    return {{"success", true}, {"message", "Document created: " + name}};
}
```

#### `open_document`
```json
{
  "name": "open_document",
  "description": "Open existing 3MF document",
  "inputSchema": {
    "type": "object",
    "properties": {
      "file_path": {"type": "string", "description": "Path to 3MF file"}
    },
    "required": ["file_path"]
  }
}
```

**✅ Implementation Strategy:**
```cpp
// Existing methods:
- Document::load(std::filesystem::path filename)
- Document::loadNonBlocking(std::filesystem::path filename)
- Document::merge(std::filesystem::path filename)  // For importing
```

**🔧 Suggested Implementation:**
```cpp
nlohmann::json handleOpenDocument(const nlohmann::json& params) {
    std::string file_path = params["file_path"];
    
    try {
        m_document->loadNonBlocking(std::filesystem::path(file_path));
        return {{"success", true}, {"message", "Opened: " + file_path}};
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    }
}
```

#### `save_document`
```json
{
  "name": "save_document",
  "description": "Save document as 3MF",
  "inputSchema": {
    "type": "object",
    "properties": {
      "file_path": {"type": "string", "description": "Output file path"},
      "format": {"type": "string", "enum": ["3mf", "stl"], "description": "Export format"},
      "with_thumbnail": {"type": "boolean", "description": "Include thumbnail"}
    },
    "required": ["file_path"]
  }
}
```

**✅ Implementation Strategy:**
```cpp
// Existing methods:
- Document::saveAs(std::filesystem::path filename, bool writeThumbnail = true)
- Document::exportAsStl(std::filesystem::path const& filename)
```

**🔧 Suggested Implementation:**
```cpp
nlohmann::json handleSaveDocument(const nlohmann::json& params) {
    std::string file_path = params["file_path"];
    std::string format = params.value("format", "3mf");
    bool with_thumbnail = params.value("with_thumbnail", true);
    
    try {
        if (format == "stl") {
            m_document->exportAsStl(std::filesystem::path(file_path));
        } else {
            m_document->saveAs(std::filesystem::path(file_path), with_thumbnail);
        }
        return {{"success", true}, {"message", "Saved: " + file_path}};
    } catch (const std::exception& e) {
        return {{"success", false}, {"error", e.what()}};
    }
}
```

### **2. Primitive Creation Tools**

The primitive creation is more complex but follows the node-based architecture. Here's what I found:

#### Key Insights:
1. **Node System**: Gladius uses a sophisticated node-based system where primitives are created as nodes
2. **SDF Functions**: Primitives are actually SDF (Signed Distance Function) nodes
3. **OpenCL Kernels**: The actual SDF calculations happen in OpenCL kernels (`sdf.cl`)

#### Available SDF Functions in `sdf.cl`:
```cpp
// Basic primitives already implemented:
float sphere(float3 pos, float radius)                    // ✅ Ready
float box(float3 pos, float3 dimensions)                  // ✅ Ready  
float cylinder(float3 pos, float radius, float height)    // ✅ Ready
float infiniteCylinder(float3 pos, float radius)          // ✅ Ready
// Torus not found - needs implementation
```

#### `create_sphere`
**✅ Implementation Strategy:**
```cpp
// Need to create node-based approach
nlohmann::json handleCreateSphere(const nlohmann::json& params) {
    std::array<float, 3> center = params["center"];
    float radius = params["radius"];
    std::string name = params.value("name", "sphere_" + generateId());
    
    // Need to create custom SDF function node
    // This requires extending the node system
    
    auto assembly = m_document->getAssembly();
    auto model = assembly->assemblyModel();
    
    // Create SDF function node with sphere expression
    std::string expression = fmt::format(
        "length(pos - vec3({}, {}, {})) - {}", 
        center[0], center[1], center[2], radius
    );
    
    // Implementation would create custom SDF node
    return createCustomSdfNode(model, name, expression);
}
```

#### **🚨 Key Implementation Challenge:**
The current node system seems designed for manual node graph creation. For MCP tools, we need:

1. **Programmatic Node Creation**: Methods to create nodes via code
2. **SDF Function Nodes**: Easy way to create custom SDF expressions
3. **Parameter Management**: Connect parameters to created nodes

### **3. CSG Operations**

#### Available CSG Functions in `sdf.cl`:
```cpp
// CSG operations already implemented in OpenCL:
float unite(float sdfA, float sdfB)                           // ✅ Ready
float uniteSmooth(float sdfA, float sdfB, float smoothing)    // ✅ Ready
float difference(float sdfA, float sdfB)                      // ✅ Ready
float differenceSmooth(float sdfA, float sdfB, float smooth)  // ✅ Ready
float intersection(float sdfA, float sdfB)                    // ✅ Ready
```

#### `csg_union`
**✅ Implementation Strategy:**
```cpp
nlohmann::json handleCsgUnion(const nlohmann::json& params) {
    std::vector<std::string> objects = params["objects"];
    bool smooth = params.value("smooth", false);
    float blend_radius = params.value("blend_radius", 0.0f);
    
    // Need to:
    // 1. Find existing nodes by name
    // 2. Create union operation node
    // 3. Connect inputs to union node
    
    auto assembly = m_document->getAssembly();
    auto model = assembly->assemblyModel();
    
    if (smooth) {
        return createSmoothUnionNode(model, objects, blend_radius);
    } else {
        return createUnionNode(model, objects);
    }
}
```

### **4. Transformations**

#### `transform_object`
**✅ Implementation Strategy:**
```cpp
// Existing transformation support:
float3 translate3f(float3 pos, float3 translation)    // ✅ In sdf.cl
// Matrix transformations available in nodes system
```

#### `set_parameter`
**✅ Implementation Strategy:**
```cpp
// Existing parameter system:
float Document::getFloatParameter(ResourceId modelId, 
                                 std::string const& nodeName,
                                 std::string const& parameterName)

void Document::setFloatParameter(ResourceId modelId,
                                std::string const& nodeName, 
                                std::string const& parameterName,
                                float value)
```

**🔧 Suggested Implementation:**
```cpp
nlohmann::json handleSetParameter(const nlohmann::json& params) {
    std::string parameter_name = params["parameter_name"];
    float value = params["value"];
    std::string object_scope = params.value("object_scope", "");
    
    // Need to map parameter names to actual node/parameter combinations
    // This requires a parameter registry or naming convention
    
    if (object_scope.empty()) {
        // Global parameter - need global parameter management
        return setGlobalParameter(parameter_name, value);
    } else {
        // Object-specific parameter
        return setObjectParameter(object_scope, parameter_name, value);
    }
}
```

### **5. Analysis Tools**

#### `analyze_geometry`
**✅ Implementation Strategy:**
```cpp
// Existing analysis methods:
BoundingBox Document::computeBoundingBox() const              // ✅ Ready
gladius::Mesh Document::generateMesh() const                 // ✅ Ready for volume calc
```

#### `get_scene_hierarchy`
**✅ Implementation Strategy:**
```cpp
// Existing scene access:
nodes::SharedAssembly Document::getAssembly() const          // ✅ Ready
// Assembly contains all models and nodes
```

### **6. Export Tools**

#### `export_mesh`
**✅ Implementation Strategy:**
```cpp
// Existing export methods:
Document::exportAsStl(std::filesystem::path const& filename)  // ✅ STL ready
vdb::exportSdfAsSTL(PreComputedSdf& sdf, std::filesystem::path) // ✅ VDB export
vdb::generatePreviewMesh(ComputeCore& generator, nodes::Assembly&) // ✅ Mesh gen
```

## Implementation Recommendations

### **Phase 1A: Core Infrastructure (Week 1-2)**

1. **Extend MCPServer.cpp**:
   ```cpp
   // Add to MCPServer class:
   std::shared_ptr<Document> m_document;
   std::map<std::string, std::function<nlohmann::json(const nlohmann::json&)>> m_toolHandlers;
   ```

2. **Create MCPToolHelpers.cpp**:
   ```cpp
   class MCPToolHelpers {
   public:
       static nlohmann::json createCustomSdfNode(nodes::Model* model, 
                                                 const std::string& name,
                                                 const std::string& expression);
       static std::vector<nodes::NodeBase*> findNodesByName(nodes::Assembly* assembly,
                                                            const std::string& name);
       // ... other helper methods
   };
   ```

### **Phase 1B: Document Management (Week 3)**

✅ **Ready to implement** - all underlying methods exist:
- `create_document` → `Document::newEmptyModel()`, `Document::newFromTemplate()`
- `open_document` → `Document::loadNonBlocking()`
- `save_document` → `Document::saveAs()`, `Document::exportAsStl()`

### **Phase 1C: Node Creation System (Week 4-5)**

🚨 **Requires new infrastructure**:
1. **Programmatic Node Creation**: Need helper methods to create nodes via code
2. **SDF Expression Builder**: Easy way to create custom SDF function nodes
3. **Node Naming/Registry**: Map human-readable names to node IDs

### **Phase 1D: Parameter Management (Week 6)**

✅ **Partially ready**:
- Parameter get/set methods exist
- Need parameter discovery and naming convention
- Need global parameter management

### **Phase 1E: Analysis & Export (Week 7)**

✅ **Ready to implement**:
- Geometry analysis methods exist
- Mesh generation available
- Export functionality ready

## Key Architecture Decisions Needed

### **1. Node Naming Strategy**
```cpp
// Option A: Name-based registry
std::map<std::string, NodeId> m_nodeNameRegistry;

// Option B: Metadata on nodes
node->setUserData("mcp_name", name);

// Option C: Special MCP nodes with built-in names
class MCPSphereNode : public nodes::SphereNode {
    std::string m_mcpName;
};
```

### **2. Parameter Management**
```cpp
// Global parameters
class MCPParameterManager {
    std::map<std::string, float> m_globalParameters;
    
public:
    void setGlobalParameter(const std::string& name, float value);
    float getGlobalParameter(const std::string& name);
    void applyToNode(NodeBase* node, const std::string& paramName);
};
```

### **3. SDF Expression Builder**
```cpp
class SDFExpressionBuilder {
public:
    static std::string sphere(const std::array<float,3>& center, float radius) {
        return fmt::format("length(pos - vec3({},{},{})) - {}", 
                          center[0], center[1], center[2], radius);
    }
    
    static std::string box(const std::array<float,3>& center, 
                          const std::array<float,3>& dimensions) {
        return fmt::format("box(pos - vec3({},{},{}), vec3({},{},{}))",
                          center[0], center[1], center[2],
                          dimensions[0]/2, dimensions[1]/2, dimensions[2]/2);
    }
};
```

## Implementation Priority

### **Phase 1 MVP Tools (Recommended order):**

1. **Document Management** (✅ Ready - 3 tools)
   - Implement using existing Document methods
   - Test with file operations

2. **Parameter Management** (✅ Mostly ready - 1 tool)
   - Build on existing parameter system
   - Add parameter discovery

3. **Analysis Tools** (✅ Ready - 2 tools)
   - Use existing analysis methods
   - Add scene hierarchy introspection

4. **Export Tools** (✅ Ready - 1 tool)
   - Leverage existing mesh export
   - Add format validation

5. **Primitive Creation** (🚨 Requires infrastructure - 4 tools)
   - Build node creation helpers
   - Implement SDF expression system

6. **CSG Operations** (🚨 Requires infrastructure - 3 tools)
   - Build on primitive creation
   - Create CSG node helpers

**Total: 14 tools for Phase 1 MVP**

This phased approach allows testing and validation of the MCP infrastructure with simpler tools before tackling the more complex node creation system.
