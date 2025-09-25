# MCP Tool Set Improvements for AI Agent Workflow

## Analysis of Current State

The current MCP server has 30+ tools, which can be overwhelming for AI agents. Based on the user guide insights, we should organize tools around the core Gladius workflow:

1. **Functions** → 2. **Level Sets** → 3. **Build Items** → 4. **Export**

## Recommended Tool Reorganization

### Core Workflow Tools (Essential - Phase 1)

These tools follow the main Gladius workflow and should be the primary interface for AI agents:

#### 1. Project Management
- `create_new_project` - Create new project (loads template.3mf automatically)
- `open_document` - Open existing document  
- `save_document` - Save current document
- `get_project_status` - Get current project state and available elements

#### 2. Function Creation (Core of Gladius workflow)
- `create_function_from_expression` - Mathematical expressions (most flexible)
- `create_box_function` - Box SDF using proper formula from user guide
- `create_sphere_function` - Sphere SDF 
- `create_gyroid_function` - Gyroid lattice structures

#### 3. Boolean Operations (Essential for combining shapes)
- `create_union_function` - Combine shapes using Min operation
- `create_intersection_function` - Common areas using Max operation  
- `create_difference_function` - Subtract shapes using Max(A, -B)

#### 4. Preview and Export
- `generate_preview` - Generate mesh preview for visualization
- `export_3mf` - Export final 3MF file for 3D printing

### Advanced Tools (Phase 2 - Expert Usage)

These tools provide advanced functionality for experienced users:

#### Advanced Geometry
- `create_cylinder_function`
- `create_torus_function` 
- `create_transform_function` - Apply transformations
- `create_noise_displacement` - Add procedural noise

#### Analysis and Validation
- `analyze_geometry` - Check geometry properties
- `validate_3mf_compliance` - Ensure 3MF standard compliance
- `validate_manufacturing` - Check 3D printing constraints

#### Parameter Management
- `set_parameter` - Modify function parameters
- `get_parameter` - Read function parameters

### Utility Tools (Always Available)

Basic tools for connectivity and diagnostics:
- `ping` - Test connectivity
- `get_status` - Application status
- `list_tools` - Show available tools

## Improved Tool Descriptions

Each tool description should include:

1. **Workflow Context**: Where this fits in the Gladius workflow
2. **Prerequisites**: What must exist before using this tool
3. **Next Steps**: What to do after using this tool
4. **Examples**: Concrete usage examples

### Example: Improved `create_box_function` Description

```json
{
  "name": "create_box_function",
  "description": "Create a box-shaped function using signed distance field (SDF). This is Step 2 in the Gladius workflow after creating a project.\n\nWorkflow Context:\n- Prerequisite: Active project (use create_new_project first)\n- Creates: A named function that defines a box shape\n- Next Steps: Use boolean operations to combine with other functions, or export directly\n\nImplementation: Uses the standard box SDF formula: length(max(abs(position - center) - size/2, 0.0))\n\nExamples:\n- Basic box: center=[0,0,0], size=[2,2,2] creates a 2×2×2 box\n- Combine with create_union_function to merge with other shapes",
  "parameters": {
    "name": "Function name (will appear in Gladius outline)",
    "center": "Box center coordinates [x,y,z]", 
    "size": "Box dimensions [width,height,depth]"
  }
}
```

## Implementation Strategy

### Phase 1: Simplify Core Workflow (High Priority)

1. **Remove overwhelming tool count** by grouping similar tools
2. **Add workflow guidance** to each tool description
3. **Create tool categories** in the MCP response to help AI agents understand structure

### Phase 2: Enhance Tool Intelligence (Medium Priority)

1. **Add prerequisite checking** - tools should report if prerequisites are missing
2. **Provide next-step suggestions** - each tool suggests what to do next  
3. **Add workflow validation** - ensure users follow logical steps

### Phase 3: Advanced AI Integration (Low Priority)

1. **Create workflow macros** - single tools that perform multi-step operations
2. **Add smart defaults** - tools suggest reasonable parameter values
3. **Implement learning** - tools adapt based on usage patterns

## Recommended Tool Categories Response

The `list_tools` response should organize tools by workflow phase:

```json
{
  "tools": {
    "workflow_phase_1_project": [
      {"name": "create_new_project", "description": "...", "workflow_step": 1},
      {"name": "get_project_status", "description": "...", "workflow_step": 0}
    ],
    "workflow_phase_2_functions": [
      {"name": "create_function_from_expression", "description": "...", "workflow_step": 2},
      {"name": "create_box_function", "description": "...", "workflow_step": 2}
    ],
    "workflow_phase_3_boolean_operations": [
      {"name": "create_union_function", "description": "...", "workflow_step": 3}
    ],
    "workflow_phase_4_export": [
      {"name": "export_3mf", "description": "...", "workflow_step": 4}
    ],
    "utility": [
      {"name": "ping", "description": "...", "workflow_step": -1}
    ]
  },
  "workflow_guide": {
    "typical_sequence": [
      "create_new_project",
      "create_box_function", 
      "create_gyroid_function",
      "create_union_function",
      "export_3mf"
    ],
    "description": "Template project already contains Level Set and Build Item - just create functions and export!"
  }
}
```

## Benefits for AI Agents

1. **Clear Workflow**: AI agents understand the logical sequence of operations
2. **Reduced Confusion**: Fewer tools means less decision paralysis
3. **Better Guidance**: Each tool explains its role in the overall workflow
4. **Prerequisite Awareness**: Tools check and report missing requirements
5. **Next Steps**: Tools suggest what to do next, guiding the workflow

## Implementation Files to Modify

1. `src/mcp/MCPServer.cpp` - Update tool descriptions and organization
2. `src/mcp/MCPServer.h` - Add workflow metadata to tool structure
3. Documentation - Update MCP documentation with workflow examples

This reorganization transforms the MCP server from a collection of disparate tools into a guided workflow system that helps AI agents understand and use Gladius effectively.
