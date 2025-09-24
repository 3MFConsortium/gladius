# Comprehensive MCP Tools Plan for Gladius: AI-Driven Implicit Modeling

## Executive Summary

Gladius is a **volumetric 3D modeling application** that specializes in **implicit surfaces** using **signed distance functions (SDFs)** and **constructive solid geometry (CSG)**. This plan outlines a comprehensive set of MCP tools that would enable AI agents to perform sophisticated 3D design workflows automatically.

## Understanding Gladius

### Core Capabilities
- **Implicit Modeling**: Uses mathematical functions to define 3D shapes
- **SDF-based Geometry**: Signed distance fields for smooth, organic shapes
- **CSG Operations**: Boolean operations (union, intersection, difference)
- **Volumetric 3MF Support**: Native support for 3MF volumetric extension
- **GPU Acceleration**: OpenCL-based computation for real-time feedback
- **Level Set Surfaces**: Perfect for organic, flowing geometries
- **Mesh Generation**: Converts implicit surfaces to triangle meshes

### Key Workflows
1. **Parametric Design**: Function-based shape creation
2. **Iterative Refinement**: Real-time parameter adjustment
3. **Complex Assembly**: Combining multiple implicit primitives
4. **Mesh Export**: STL, 3MF, VDB output for 3D printing/manufacturing

## Comprehensive MCP Tool Suite

### 1. **Document Management Tools**

#### `create_document`
```json
{
  "name": "create_document",
  "description": "Create a new Gladius document for implicit modeling",
  "inputSchema": {
    "type": "object",
    "properties": {
      "name": {"type": "string", "description": "Document name"},
      "template": {"type": "string", "enum": ["empty", "basic_csg", "organic_shapes"], "description": "Starting template"}
    },
    "required": ["name"]
  }
}
```

#### `open_document`
```json
{
  "name": "open_document", 
  "description": "Open existing 3MF document with volumetric extension",
  "inputSchema": {
    "type": "object",
    "properties": {
      "file_path": {"type": "string", "description": "Path to 3MF file"},
      "validate_volumetric": {"type": "boolean", "description": "Validate volumetric extension compliance"}
    },
    "required": ["file_path"]
  }
}
```

#### `save_document`
```json
{
  "name": "save_document",
  "description": "Save current document as 3MF with volumetric extension",
  "inputSchema": {
    "type": "object", 
    "properties": {
      "file_path": {"type": "string", "description": "Output file path"},
      "format": {"type": "string", "enum": ["3mf", "stl", "vdb"], "description": "Export format"},
      "quality": {"type": "string", "enum": ["draft", "normal", "high"], "description": "Mesh quality for export"}
    },
    "required": ["file_path"]
  }
}
```

### 2. **Primitive Creation Tools**

#### `create_sphere`
```json
{
  "name": "create_sphere",
  "description": "Create a sphere primitive using SDF",
  "inputSchema": {
    "type": "object",
    "properties": {
      "center": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "X, Y, Z coordinates"},
      "radius": {"type": "number", "minimum": 0, "description": "Sphere radius in mm"},
      "name": {"type": "string", "description": "Node name in assembly"}
    },
    "required": ["center", "radius"]
  }
}
```

#### `create_box`
```json
{
  "name": "create_box", 
  "description": "Create a box primitive using SDF",
  "inputSchema": {
    "type": "object",
    "properties": {
      "center": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
      "dimensions": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "Width, Height, Depth in mm"},
      "rounding": {"type": "number", "minimum": 0, "description": "Corner rounding radius"}
    },
    "required": ["center", "dimensions"]
  }
}
```

#### `create_cylinder`
```json
{
  "name": "create_cylinder",
  "description": "Create a cylinder primitive using SDF", 
  "inputSchema": {
    "type": "object",
    "properties": {
      "base_center": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
      "height": {"type": "number", "minimum": 0, "description": "Cylinder height in mm"},
      "radius": {"type": "number", "minimum": 0, "description": "Cylinder radius in mm"},
      "axis": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "Cylinder axis direction"}
    },
    "required": ["base_center", "height", "radius"]
  }
}
```

#### `create_torus`
```json
{
  "name": "create_torus",
  "description": "Create a torus primitive using SDF",
  "inputSchema": {
    "type": "object", 
    "properties": {
      "center": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
      "major_radius": {"type": "number", "minimum": 0, "description": "Major radius (center to tube center)"},
      "minor_radius": {"type": "number", "minimum": 0, "description": "Minor radius (tube thickness)"}
    },
    "required": ["center", "major_radius", "minor_radius"]
  }
}
```

### 3. **CSG Boolean Operations**

#### `csg_union`
```json
{
  "name": "csg_union",
  "description": "Combine two or more objects using CSG union", 
  "inputSchema": {
    "type": "object",
    "properties": {
      "objects": {"type": "array", "items": {"type": "string"}, "minItems": 2, "description": "Object names to unite"},
      "smooth": {"type": "boolean", "description": "Use smooth union for organic blending"},
      "blend_radius": {"type": "number", "minimum": 0, "description": "Blending radius for smooth union"}
    },
    "required": ["objects"]
  }
}
```

#### `csg_difference`
```json
{
  "name": "csg_difference",
  "description": "Subtract objects using CSG difference",
  "inputSchema": {
    "type": "object",
    "properties": {
      "base_object": {"type": "string", "description": "Object to subtract from"},
      "subtract_objects": {"type": "array", "items": {"type": "string"}, "description": "Objects to subtract"},
      "smooth": {"type": "boolean", "description": "Use smooth difference"}
    },
    "required": ["base_object", "subtract_objects"] 
  }
}
```

#### `csg_intersection`
```json
{
  "name": "csg_intersection", 
  "description": "Find intersection of objects using CSG",
  "inputSchema": {
    "type": "object",
    "properties": {
      "objects": {"type": "array", "items": {"type": "string"}, "minItems": 2, "description": "Objects to intersect"},
      "smooth": {"type": "boolean", "description": "Use smooth intersection"}
    },
    "required": ["objects"]
  }
}
```

### 4. **Transformation Tools**

#### `transform_object`
```json
{
  "name": "transform_object",
  "description": "Apply transformations to objects",
  "inputSchema": {
    "type": "object",
    "properties": {
      "object_name": {"type": "string", "description": "Object to transform"},
      "translation": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
      "rotation": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "Rotation in degrees (X, Y, Z)"},
      "scale": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "Scale factors (X, Y, Z)"}
    },
    "required": ["object_name"]
  }
}
```

#### `array_object`
```json
{
  "name": "array_object",
  "description": "Create arrays/patterns of objects",
  "inputSchema": {
    "type": "object", 
    "properties": {
      "object_name": {"type": "string", "description": "Object to array"},
      "pattern": {"type": "string", "enum": ["linear", "circular", "grid"], "description": "Array pattern type"},
      "count": {"type": "integer", "minimum": 1, "description": "Number of copies"},
      "spacing": {"type": "array", "items": {"type": "number"}, "description": "Spacing between copies"},
      "axis": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "Array axis for circular patterns"}
    },
    "required": ["object_name", "pattern", "count"]
  }
}
```

### 5. **Advanced Implicit Modeling Tools**

#### `create_metaball`
```json
{
  "name": "create_metaball",
  "description": "Create metaball/blob primitive for organic modeling",
  "inputSchema": {
    "type": "object",
    "properties": {
      "centers": {"type": "array", "items": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3}, "description": "Metaball center points"},
      "radii": {"type": "array", "items": {"type": "number"}, "description": "Influence radius for each center"},
      "threshold": {"type": "number", "description": "Iso-surface threshold value"}
    },
    "required": ["centers", "radii"]
  }
}
```

#### `create_noise_displacement`
```json
{
  "name": "create_noise_displacement",
  "description": "Apply procedural noise displacement to surfaces",
  "inputSchema": {
    "type": "object",
    "properties": {
      "target_object": {"type": "string", "description": "Object to displace"},
      "noise_type": {"type": "string", "enum": ["perlin", "simplex", "worley"], "description": "Noise algorithm"},
      "amplitude": {"type": "number", "description": "Displacement strength"},
      "frequency": {"type": "number", "description": "Noise frequency/scale"},
      "octaves": {"type": "integer", "minimum": 1, "maximum": 8, "description": "Noise detail levels"}
    },
    "required": ["target_object", "noise_type", "amplitude"]
  }
}
```

#### `create_sdf_function`
```json
{
  "name": "create_sdf_function",
  "description": "Create custom SDF using mathematical expression",
  "inputSchema": {
    "type": "object",
    "properties": {
      "name": {"type": "string", "description": "Function name"},
      "expression": {"type": "string", "description": "Mathematical SDF expression (using pos.x, pos.y, pos.z)"},
      "parameters": {"type": "object", "description": "Named parameters for the expression"},
      "bounding_box": {"type": "array", "items": {"type": "number"}, "minItems": 6, "maxItems": 6, "description": "Min/max bounds [minX, minY, minZ, maxX, maxY, maxZ]"}
    },
    "required": ["name", "expression"]
  }
}
```

### 6. **Analysis and Inspection Tools**

#### `analyze_geometry`
```json
{
  "name": "analyze_geometry",
  "description": "Analyze geometric properties of objects",
  "inputSchema": {
    "type": "object",
    "properties": {
      "object_name": {"type": "string", "description": "Object to analyze"},
      "analysis_type": {"type": "array", "items": {"type": "string", "enum": ["volume", "surface_area", "bounding_box", "center_of_mass", "moments"]}, "description": "Types of analysis to perform"}
    },
    "required": ["object_name", "analysis_type"]
  }
}
```

#### `get_scene_hierarchy`
```json
{
  "name": "get_scene_hierarchy", 
  "description": "Get the complete scene hierarchy and object relationships",
  "inputSchema": {
    "type": "object",
    "properties": {
      "include_properties": {"type": "boolean", "description": "Include detailed object properties"},
      "format": {"type": "string", "enum": ["tree", "flat", "json"], "description": "Output format"}
    },
    "required": []
  }
}
```

#### `measure_distance`
```json
{
  "name": "measure_distance",
  "description": "Measure distances between objects or points",
  "inputSchema": {
    "type": "object",
    "properties": {
      "from": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "Start point or object name"},
      "to": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3, "description": "End point or object name"},
      "measurement_type": {"type": "string", "enum": ["straight", "surface", "volume"], "description": "Type of distance measurement"}
    },
    "required": ["from", "to"]
  }
}
```

### 7. **Rendering and Visualization Tools**

#### `generate_preview`
```json
{
  "name": "generate_preview", 
  "description": "Generate preview mesh for visualization",
  "inputSchema": {
    "type": "object",
    "properties": {
      "objects": {"type": "array", "items": {"type": "string"}, "description": "Objects to include (empty for all)"},
      "quality": {"type": "string", "enum": ["draft", "preview", "high"], "description": "Mesh generation quality"},
      "resolution": {"type": "integer", "minimum": 32, "maximum": 512, "description": "Voxel resolution"}
    },
    "required": []
  }
}
```

#### `render_image`
```json
{
  "name": "render_image",
  "description": "Render high-quality image of the model",
  "inputSchema": {
    "type": "object",
    "properties": {
      "camera_position": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
      "camera_target": {"type": "array", "items": {"type": "number"}, "minItems": 3, "maxItems": 3},
      "resolution": {"type": "array", "items": {"type": "integer"}, "minItems": 2, "maxItems": 2, "description": "Width, height in pixels"},
      "lighting": {"type": "string", "enum": ["studio", "outdoor", "dramatic"], "description": "Lighting setup"},
      "output_path": {"type": "string", "description": "Path to save rendered image"}
    },
    "required": ["resolution", "output_path"]
  }
}
```

### 8. **Parameter and Expression Management**

#### `set_parameter`
```json
{
  "name": "set_parameter",
  "description": "Set global or object-specific parameters",
  "inputSchema": {
    "type": "object",
    "properties": {
      "parameter_name": {"type": "string", "description": "Parameter identifier"},
      "value": {"type": "number", "description": "Parameter value"},
      "object_scope": {"type": "string", "description": "Object to apply parameter to (empty for global)"}
    },
    "required": ["parameter_name", "value"]
  }
}
```

#### `get_parameters`
```json
{
  "name": "get_parameters",
  "description": "Get all available parameters and their current values",
  "inputSchema": {
    "type": "object",
    "properties": {
      "object_name": {"type": "string", "description": "Object to get parameters for (empty for global)"},
      "include_expressions": {"type": "boolean", "description": "Include parameter expressions/formulas"}
    },
    "required": []
  }
}
```

### 9. **Import/Export and Mesh Processing**

#### `import_mesh`
```json
{
  "name": "import_mesh",
  "description": "Import external mesh and convert to SDF representation",
  "inputSchema": {
    "type": "object",
    "properties": {
      "file_path": {"type": "string", "description": "Path to mesh file (STL, OBJ, PLY)"},
      "voxel_resolution": {"type": "integer", "minimum": 32, "description": "SDF voxelization resolution"},
      "name": {"type": "string", "description": "Name for imported object"}
    },
    "required": ["file_path"]
  }
}
```

#### `export_mesh`
```json
{
  "name": "export_mesh",
  "description": "Export implicit surfaces as triangle mesh",
  "inputSchema": {
    "type": "object",
    "properties": {
      "objects": {"type": "array", "items": {"type": "string"}, "description": "Objects to export"},
      "file_path": {"type": "string", "description": "Output file path"},
      "format": {"type": "string", "enum": ["stl", "obj", "ply", "3mf"], "description": "Export format"},
      "quality": {"type": "string", "enum": ["draft", "normal", "high", "ultra"], "description": "Mesh quality"},
      "units": {"type": "string", "enum": ["mm", "cm", "m", "in"], "description": "Output units"}
    },
    "required": ["file_path", "format"]
  }
}
```

### 10. **AI-Specific Workflow Tools**

#### `batch_operations`
```json
{
  "name": "batch_operations",
  "description": "Execute multiple operations as atomic transaction",
  "inputSchema": {
    "type": "object",
    "properties": {
      "operations": {"type": "array", "items": {"type": "object"}, "description": "List of operations to execute"},
      "rollback_on_error": {"type": "boolean", "description": "Rollback all changes if any operation fails"}
    },
    "required": ["operations"]
  }
}
```

#### `validate_design`
```json
{
  "name": "validate_design",
  "description": "Validate design for 3D printing and manufacturing",
  "inputSchema": {
    "type": "object",
    "properties": {
      "check_types": {"type": "array", "items": {"type": "string", "enum": ["printability", "manifold", "thickness", "overhangs", "supports"]}, "description": "Validation checks to perform"},
      "printer_constraints": {"type": "object", "description": "3D printer limitations and requirements"}
    },
    "required": ["check_types"]
  }
}
```

## AI Workflow Examples

### 1. **Generative Design Workflow**
```python
# AI agent creates parametric chair design
agent.create_document("ergonomic_chair")
agent.set_parameter("seat_width", 400)
agent.set_parameter("back_height", 800)

# Create basic structure
agent.create_box([0,0,200], [400,400,50], name="seat")  
agent.create_box([0,200,400], [380,50,400], name="backrest")

# Add comfort features
agent.create_metaball([[0,0,250]], [150], name="lumbar_support")
agent.csg_union(["backrest", "lumbar_support"], smooth=True)

# Validate and iterate
results = agent.validate_design(["printability", "ergonomics"])
if results["issues"]:
    agent.adjust_parameters_based_on_feedback(results)
```

### 2. **Organic Architecture Workflow**
```python
# AI creates flowing architectural element
agent.create_sdf_function("flowing_column", 
    "smoothstep(0, 100, length(pos.xz)) - sin(pos.y * 0.1) * 20")

agent.create_noise_displacement("flowing_column", 
    noise_type="perlin", amplitude=5, frequency=0.05)

# Generate optimized structure
mesh = agent.generate_preview(quality="high")
analysis = agent.analyze_geometry("flowing_column", ["structural_integrity"])
```

### 3. **Product Design Iteration**
```python
# AI iterates on product design based on feedback
for iteration in range(10):
    # Modify parameters based on performance
    agent.set_parameter("wall_thickness", base_thickness + iteration * 0.1)
    
    # Test design
    mesh = agent.generate_preview(quality="draft")
    analysis = agent.analyze_geometry("product", ["volume", "weight"])
    
    if analysis["weight"] < target_weight:
        break
        
# Final high-quality export
agent.export_mesh(["product"], "final_design.stl", quality="ultra")
```

## Implementation Priority

### **Phase 1: Core Foundation** 
- Document management (create, open, save)
- Basic primitives (sphere, box, cylinder)
- Simple CSG operations (union, difference, intersection)
- Basic transformations

### **Phase 2: Essential Modeling**
- Advanced primitives (torus, metaballs)
- Smooth CSG operations
- Parameter management
- Scene hierarchy inspection

### **Phase 3: AI Enablement**
- SDF functions and expressions
- Batch operations
- Analysis tools
- Validation systems

### **Phase 4: Advanced Features**
- Noise displacement and procedural modeling
- Advanced rendering
- Import/export workflows
- Manufacturing validation

## Conclusion

This comprehensive MCP tool suite would enable AI agents to:
- **Design parametrically** using mathematical expressions
- **Iterate rapidly** with real-time feedback
- **Create organic forms** impossible with traditional CAD
- **Validate automatically** for manufacturing constraints
- **Generate variations** for optimization
- **Export ready-to-print** models

The implicit modeling paradigm of Gladius is **perfect for AI-driven design** because it allows mathematical, procedural approaches that AI agents can understand and manipulate effectively.
