# MCP Tools Quick Reference for AI Agents

## Core Workflow Tools

### Document Management
```python
# Create new document
create_document(name="my_design", template="empty")

# Open existing 3MF file  
open_document(file_path="/path/to/model.3mf")

# Save current work
save_document(file_path="/path/to/output.3mf", format="3mf")
```

### Primitive Creation
```python
# Create sphere
create_sphere(center=[0,0,0], radius=50, name="sphere1")

# Create box with rounded corners
create_box(center=[0,0,0], dimensions=[100,80,60], rounding=5)

# Create cylinder
create_cylinder(base_center=[0,0,0], height=100, radius=25)

# Create torus
create_torus(center=[0,0,0], major_radius=50, minor_radius=10)
```

### CSG Operations
```python
# Combine objects smoothly
csg_union(objects=["sphere1", "box1"], smooth=True, blend_radius=5)

# Subtract objects
csg_difference(base_object="box1", subtract_objects=["cylinder1"])

# Find intersection
csg_intersection(objects=["sphere1", "box1"], smooth=False)
```

### Transformations
```python
# Move, rotate, scale objects
transform_object(object_name="sphere1", 
                translation=[10,0,0], 
                rotation=[0,0,45], 
                scale=[1.5,1,1])

# Set parameters for parametric control
set_parameter(parameter_name="wall_thickness", value=2.5)
```

## Advanced Features

### Metaballs (Organic Shapes)
```python
# Create organic blob shapes
create_metaball(centers=[[0,0,0], [30,0,0], [15,15,0]], 
               radii=[20,15,10], 
               threshold=0.5)
```

### Custom SDF Functions
```python
# Create custom mathematical shapes
create_sdf_function(name="twisted_box", 
                   expression="box(pos - vec3(0,0,0), vec3(10,10,pos.y*0.1))")
```

### Procedural Displacement
```python
# Add noise texture to surfaces
create_noise_displacement(target_object="sphere1", 
                         noise_type="perlin", 
                         amplitude=5, 
                         frequency=0.1)
```

## Analysis Tools

### Geometry Analysis
```python
# Get object properties
analyze_geometry(object_name="sphere1", 
                analysis_type=["volume", "surface_area", "bounding_box"])

# Get scene structure
get_scene_hierarchy(include_properties=True, format="json")

# Measure distances
measure_distance(from=[0,0,0], to=[100,0,0], measurement_type="straight")
```

### Validation
```python
# Check 3D printing compatibility
validate_design(check_types=["printability", "manifold", "thickness"])
```

## Export and Visualization

### Preview Generation
```python
# Generate mesh for visualization
generate_preview(objects=["sphere1", "box1"], quality="high", resolution=256)

# Render image
render_image(camera_position=[100,100,100], 
            resolution=[1920,1080], 
            output_path="/path/to/render.png")
```

### Export Options
```python
# Export for 3D printing
export_mesh(objects=["final_design"], 
           file_path="/path/to/model.stl", 
           format="stl", 
           quality="high")

# Import existing mesh
import_mesh(file_path="/path/to/scan.stl", voxel_resolution=128)
```

## Batch Operations for AI

### Complex Workflows
```python
# Execute multiple operations atomically
batch_operations(operations=[
    {"tool": "create_sphere", "params": {"center": [0,0,0], "radius": 25}},
    {"tool": "create_box", "params": {"center": [0,0,50], "dimensions": [50,50,20]}},
    {"tool": "csg_union", "params": {"objects": ["sphere", "box"], "smooth": True}}
], rollback_on_error=True)
```

## Typical AI Workflows

### Generative Design Pattern
```python
# 1. Create base geometry
create_sphere(center=[0,0,0], radius=50)

# 2. Add features iteratively  
create_cylinder(base_center=[0,0,25], height=50, radius=10)
csg_difference(base_object="sphere", subtract_objects=["cylinder"])

# 3. Analyze and optimize
analysis = analyze_geometry("sphere", ["volume", "surface_area"])
if analysis["volume"] > target_volume:
    set_parameter("radius", 45)  # Reduce size

# 4. Validate for manufacturing
validation = validate_design(["printability", "supports"])

# 5. Export final result
export_mesh(["sphere"], "final_design.stl", quality="ultra")
```

### Parametric Iteration Pattern
```python
# Set up parametric model
set_parameter("length", 100)
set_parameter("width", 50) 
set_parameter("height", 20)

# Create parametric geometry using expressions
create_sdf_function("parametric_box", 
    "box(pos, vec3(length/2, width/2, height/2))")

# Iterate parameters based on analysis
for iteration in range(10):
    analysis = analyze_geometry("parametric_box", ["volume"])
    if analysis["volume"] < target_volume:
        set_parameter("length", get_parameter("length") * 1.1)
    else:
        break
```

### Organic Modeling Pattern
```python
# Create organic base forms
create_metaball(centers=[[0,0,0], [40,20,0], [-20,30,10]], 
               radii=[30,25,20])

# Add surface detail
create_noise_displacement("metaball", 
                         noise_type="perlin", 
                         amplitude=3, 
                         frequency=0.05)

# Refine with boolean operations
create_sphere(center=[0,0,0], radius=60)
csg_intersection(["metaball", "sphere"])  # Trim to sphere
```

## Best Practices for AI Agents

### 1. **Always Name Objects**
```python
create_sphere(center=[0,0,0], radius=50, name="main_body")
# Better than letting system auto-name
```

### 2. **Use Parametric Approach**
```python
set_parameter("radius", 50)
create_sphere(center=[0,0,0], radius="radius")  # Reference parameter
# Enables easy iteration
```

### 3. **Validate Before Export**
```python
validation = validate_design(["printability", "manifold"])
if validation["valid"]:
    export_mesh(["design"], "output.stl")
else:
    # Fix issues first
    fix_design_issues(validation["issues"])
```

### 4. **Use Batch Operations for Complex Changes**
```python
batch_operations([
    # Multiple related operations
], rollback_on_error=True)
# Ensures consistency
```

### 5. **Progressive Quality**
```python
# Draft for iteration
generate_preview(quality="draft")  

# High quality for final
generate_preview(quality="high")
export_mesh(quality="ultra")
```

This reference guide provides the essential tools and patterns for AI agents to effectively control Gladius for sophisticated 3D modeling workflows.
