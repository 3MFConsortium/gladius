# 3MF Volumetric Extension Architecture in Gladius

## Overview

This document describes the technical implementation of the 3MF Volumetric Extension within Gladius, covering the architecture, data structures, and implementation details for working with implicit geometries and volumetric data.

## 3MF Volumetric Extension Structure

### Namespace Hierarchy

The 3MF Volumetric Extension consists of two complementary namespaces:

1. **Volumetric Namespace (`v:`)**: `http://schemas.3mf.io/3dmanufacturing/volumetric/2022/01`
2. **Implicit Namespace (`i:`)**: `http://schemas.3mf.io/3dmanufacturing/implicit/2023/12`

### Core Components

#### 1. Functions (`v:function`)

Functions are the fundamental building blocks for volumetric evaluation:

```xml
<v:function id="1" displayname="My Function">
  <v:functionfromimage3d image3did="2" filter="linear" 
                         tilestyleu="wrap" tilestylev="clamp" tilestylew="mirror"/>
</v:function>
```

**Function Types:**

- **FunctionFromImage3D**: Evaluates 3D image data as implicit functions
- **ImplicitFunction**: Node-based mathematical function graphs
- **PrivateExtensionFunction**: Custom function implementations

#### 2. Level Sets (`v:levelset`)

Level sets define surfaces using implicit functions:

```xml
<v:levelset functionid="1" channel="result" transform="1 0 0 0 1 0 0 0 1 0 0 0"
            minfeaturesize="0.1" meshbboxonly="false" fallbackvalue="0.0"/>
```

**Key Properties:**

- `functionid`: Reference to the function resource
- `channel`: Output channel to use for surface definition
- `transform`: 3D transformation matrix (4x3 format)
- `minfeaturesize`: Minimum feature size for mesh generation
- `meshbboxonly`: Restrict evaluation to mesh bounding box
- `fallbackvalue`: Default value for undefined regions

#### 3. Volumetric Data (`v:volumedata`)

Defines spatially varying properties throughout the volume:

```xml
<v:volumedata id="3">
  <v:color functionid="4" channel="rgb" transform="1 0 0 0 1 0 0 0 1 0 0 0"/>
  <v:composite basematerialid="1">
    <v:materialmapping functionid="5" channel="weight"/>
  </v:composite>
  <v:property name="density" functionid="6" channel="value" required="true"/>
</v:volumedata>
```

### Implicit Function Node Graph Architecture

#### Function Definition

```xml
<i:implicitfunction id="2" displayname="Sphere Function">
  <i:in>
    <i:vector identifier="pos" displayname="Position"/>
    <i:scalar identifier="radius" displayname="Radius"/>
  </i:in>
  
  <!-- Node graph starts here -->
  <i:length identifier="length_node" displayname="Distance from Origin">
    <i:in>
      <i:vectorref identifier="A" ref="inputs.pos"/>
    </i:in>
    <i:out>
      <i:scalar identifier="result"/>
    </i:out>
  </i:length>
  
  <i:subtraction identifier="sub_node" displayname="Subtract Radius">
    <i:in>
      <i:scalarref identifier="A" ref="length_node.result"/>
      <i:scalarref identifier="B" ref="inputs.radius"/>
    </i:in>
    <i:out>
      <i:scalar identifier="result"/>
    </i:out>
  </i:subtraction>
  
  <i:out>
    <i:scalarref identifier="distance" ref="sub_node.result"/>
  </i:out>
</i:implicitfunction>
```

#### Node Types and Operations

**Mathematical Operations:**
- `i:addition`, `i:subtraction`, `i:multiplication`, `i:division`
- `i:min`, `i:max`, `i:abs`, `i:pow`, `i:sqrt`
- `i:sin`, `i:cos`, `i:tan`, `i:arcsin`, `i:arccos`, `i:arctan`, `i:arctan2`
- `i:log`, `i:log2`, `i:log10`, `i:exp`
- `i:floor`, `i:ceil`, `i:round`, `i:sign`, `i:fract`

**Vector Operations:**
- `i:composevector`, `i:decomposevector`, `i:vectorfromscalar`
- `i:dot`, `i:cross`, `i:length`

**Matrix Operations:**
- `i:composematrix`, `i:matrixfromcolumns`, `i:matrixfromrows`
- `i:matvecmultiplication`, `i:transpose`, `i:inverse`

**Geometric Operations:**
- `i:mesh`: Signed distance to mesh
- `i:unsignedmesh`: Unsigned distance to mesh

**Control Flow:**
- `i:select`, `i:clamp`
- `i:functioncall`: Call other implicit functions

**Constants:**
- `i:constant`: Scalar constants
- `i:constvec`: Vector constants  
- `i:constmat`: Matrix constants
- `i:constresourceid`: Resource ID references

## Implementation Details in Gladius

### Function Evaluation Pipeline

1. **Function Parsing**: XML nodes are parsed into internal representation
2. **Graph Validation**: Acyclic directed graph validation
3. **OpenCL Compilation**: Node graphs compiled to OpenCL kernels
4. **GPU Execution**: Parallel evaluation across spatial domains
5. **Surface Extraction**: Marching cubes or similar algorithms for mesh generation

### Data Type System

#### Scalar (`ST_ScalarID`)
- Single floating-point values
- IEEE 754 single-precision recommended

#### Vector (`ST_VectorID`)
- 3-component floating-point vectors (x, y, z)
- Used for positions, directions, colors (RGB)

#### Matrix (`ST_MatrixID`)
- 4x4 transformation matrices
- Row-major format: `m00 m01 m02 m03 m10 m11 m12 m13 m20 m21 m22 m23 m30 m31 m32 m33`

#### Resource References (`ST_ResourceID`)
- References to other 3MF resources
- Used for function calls and mesh references

### Memory Management and Performance

#### Evaluation Strategy
```cpp
class ImplicitFunctionEvaluator {
public:
    float evaluate(const Vector3& position, const FunctionParameters& params);
    void evaluateGrid(float* output, const BoundingBox& bounds, 
                     const Vector3i& resolution, const FunctionParameters& params);
private:
    OpenCLKernel m_kernel;
    std::vector<Node> m_nodeGraph;
};
```

#### Optimization Techniques

1. **Spatial Coherence**: Exploit spatial locality for cache efficiency
2. **Early Termination**: Skip evaluation outside bounding boxes
3. **Level-of-Detail**: Adaptive resolution based on feature size
4. **GPU Parallelization**: Vectorized operations on SIMD units

### Integration with 3MF Core

#### Resource Management

Resources are managed within the 3MF resource collection:

```xml
<resources>
  <!-- Traditional mesh objects -->
  <object id="1" type="model">
    <mesh><!-- ... --></mesh>
  </object>
  
  <!-- Volumetric functions -->
  <v:function id="2"><!-- ... --></v:function>
  <i:implicitfunction id="3"><!-- ... --></i:implicitfunction>
  
  <!-- Level set objects -->
  <object id="4" type="model">
    <v:levelset functionid="3" channel="result"/>
  </object>
  
  <!-- Volumetric data -->
  <v:volumedata id="5"><!-- ... --></v:volumedata>
</resources>
```

#### Build Platform Integration

Level set objects can be added to the build platform like traditional objects:

```xml
<build>
  <item objectid="4" transform="1 0 0 10 0 1 0 20 0 0 1 30"/>
</build>
```

### Error Handling and Validation

#### Function Graph Validation

1. **Acyclic Verification**: Ensure no circular dependencies
2. **Type Checking**: Verify input/output type compatibility
3. **Reference Validation**: Check all node references are valid
4. **Completeness Check**: Ensure all inputs are connected

#### Runtime Error Handling

```cpp
enum class EvaluationResult {
    Success,
    DivisionByZero,
    InvalidInput,
    ResourceNotFound,
    ComputationError
};

class SafeEvaluator {
public:
    EvaluationResult evaluate(const Vector3& pos, float& result) {
        try {
            result = m_function.evaluate(pos);
            return EvaluationResult::Success;
        } catch (const DivisionByZeroException&) {
            result = m_fallbackValue;
            return EvaluationResult::DivisionByZero;
        }
        // ... other error cases
    }
private:
    float m_fallbackValue = 0.0f;
};
```

## Advanced Features

### Coordinate Transformations

Functions can be evaluated in different coordinate systems using transformation matrices:

```xml
<v:levelset functionid="1" channel="result" 
            transform="0.5 0 0 0 0.5 0 0 0 0.5 10 20 30"/>
```

This applies scaling (0.5x) and translation (10, 20, 30) to the function domain.

### Multi-Channel Functions

Functions can output multiple channels for different purposes:

```xml
<i:implicitfunction id="7" displayname="Multi-Channel Function">
  <i:out>
    <i:scalarref identifier="distance" ref="distance_node.result"/>
    <i:vectorref identifier="color" ref="color_node.result"/>
    <i:scalarref identifier="density" ref="density_node.result"/>
  </i:out>
</i:implicitfunction>
```

### Image3D Integration

3D images can be used as basis for implicit functions:

```xml
<v:image3d id="8" name="Density Map">
  <v:imagestack>
    <v:imagesheet path="/3D/density_slice_000.png"/>
    <v:imagesheet path="/3D/density_slice_001.png"/>
    <!-- ... more slices ... -->
  </v:imagestack>
</v:image3d>

<v:functionfromimage3d id="9" image3did="8" filter="linear" 
                       tilestyleu="wrap" tilestylev="clamp" tilestylew="mirror"
                       valueoffset="0.0" valuescale="1.0"/>
```

### Material Composition

Volumetric data can define complex material distributions:

```xml
<v:volumedata id="10">
  <v:composite basematerialid="1">
    <v:materialmapping functionid="11" channel="steel_fraction"/>
    <v:materialmapping functionid="12" channel="aluminum_fraction"/>
  </v:composite>
</v:volumedata>
```

## Best Practices

### Function Design

1. **Modularity**: Create reusable function components
2. **Parameter Exposure**: Make key parameters function inputs
3. **Numerical Stability**: Avoid operations that may cause numerical issues
4. **Performance**: Minimize complex operations in hot paths

### 3MF Compliance

1. **Namespace Declaration**: Always declare required namespaces
2. **ID Management**: Use unique resource IDs throughout the document
3. **Fallback Values**: Provide appropriate fallback values for edge cases
4. **Validation**: Validate 3MF files against the official schema

### Performance Optimization

1. **Bounding Box Usage**: Use mesh bounding boxes to limit evaluation regions
2. **Feature Size Selection**: Choose appropriate minimum feature sizes
3. **Function Complexity**: Balance detail with computational cost
4. **Memory Management**: Consider memory usage for large datasets

## Troubleshooting

### Common Issues

1. **Circular Dependencies**: Functions reference themselves directly or indirectly
2. **Type Mismatches**: Scalar outputs connected to vector inputs
3. **Missing Resources**: References to non-existent function or resource IDs
4. **Numerical Instability**: Division by zero, logarithm of negative numbers

### Debugging Strategies

1. **Incremental Building**: Test simple functions before adding complexity
2. **Visualization**: Use cross-sections to examine function behavior
3. **Parameter Sweeps**: Vary parameters to understand function sensitivity
4. **Validation Tools**: Use 3MF validation tools to check compliance

## Future Considerations

### Planned Enhancements

1. **Performance Improvements**: GPU compute shader support
2. **Advanced Materials**: Anisotropic and time-varying properties
3. **Optimization Integration**: Topology optimization result import
4. **Extended Node Library**: Additional mathematical operations

### Research Areas

1. **Adaptive Evaluation**: Dynamic resolution based on geometric complexity
2. **Machine Learning Integration**: Neural implicit functions
3. **Multi-Physics**: Coupled thermal, structural, and fluid properties
4. **Manufacturing Constraints**: Direct integration of printer capabilities

---

*This document provides technical implementation details for the 3MF Volumetric Extension in Gladius. For user-facing documentation, refer to the User Guide. For the latest specification details, consult the official 3MF Consortium documentation.*
