# 3MF Beam Lattice BVH Implementation Plan

## Executive Summary

This plan outlines the implementation of 3MF beam lattice structures with a Bounding Volume Hierarchy (BVH) for efficient distance field evaluation in ray marching applications. The implementation supports the 3MF Beam Lattice Extension specification and integrates seamlessly with Gladius's existing geometry pipeline.

## 1. Overview and Requirements

### 1.1 3MF Beam Lattice Extension Requirements

Based on the [3MF Beam Lattice Extension Specification](../docs/3MF%20Beam%20Lattice%20Extension.md), we need to support:

- **Beam geometry**: Cylindrical/conical beams with circular cross-sections
- **Beam properties**: Start/end points, start/end radii, cap styles
- **Ball geometry**: Spherical nodes at beam vertices (optional)
- **Clipping**: Against reference meshes (inside/outside/none modes)
- **Representation**: Optional mesh representation for preview
- **Properties**: Material properties per beam/ball

### 1.2 Performance Requirements

- Fast distance queries for ray marching rendering
- Support for large lattice structures (thousands of beams)
- GPU/OpenCL kernel compatibility
- CPU-side BVH construction and traversal
- Memory-efficient data structures

### 1.3 Integration Points

- Extend existing `PrimitiveMeta` structure in `types.h`
- Integrate with `Importer3mf.cpp` for 3MF file loading
- Add new `PrimitiveType` enum values for beams
- Leverage existing BVH patterns from mesh handling

## 2. Data Structure Design

### 2.1 Beam Representation

```cpp
struct BeamData
{
    float4 startPos;       // Start position (w component unused)
    float4 endPos;         // End position (w component unused) 
    float startRadius;     // Radius at start
    float endRadius;       // Radius at end
    int startCapStyle;     // Cap style: 0=hemisphere, 1=sphere, 2=butt
    int endCapStyle;       // Cap style for end
    int materialId;        // Material/property ID
    int padding;           // Alignment padding
};

struct BallData
{
    float4 position;       // Ball center (w component unused)
    float radius;          // Ball radius
    int materialId;        // Material/property ID
    int padding[2];        // Alignment padding
};
```

### 2.2 Extended PrimitiveMeta for Beams

Extend existing `PrimitiveMeta` structure:

```cpp
enum PrimitiveType
{
    // ... existing types ...
    SDF_BEAM_LATTICE,      // Beam lattice root node
    SDF_BEAM,              // Individual beam primitive
    SDF_BALL,              // Ball at beam vertex
    SDF_BEAM_BVH_NODE,     // BVH internal node for beams
};

struct PrimitiveMeta
{
    float4 center;
    int start;             // Data index or left child for BVH
    int end;               // Data count or right child for BVH  
    float scaling;
    enum PrimitiveType primitiveType;
    struct BoundingBox boundingBox;
    float4 approximationTop;
    float4 approximationBottom;
    
    // Additional beam-specific data
    int clippingMeshId;    // Reference to clipping mesh (-1 if none)
    int clippingMode;      // 0=none, 1=inside, 2=outside
    int representationMeshId; // Reference to representation mesh (-1 if none)
    int ballMode;          // 0=none, 1=mixed, 2=all
};
```

### 2.3 BVH Node Structure

```cpp
struct BeamBVHNode
{
    BoundingBox boundingBox;
    int leftChild;         // Index to left child (-1 if leaf)
    int rightChild;        // Index to right child (-1 if leaf)
    int primitiveStart;    // First primitive index (for leaves)
    int primitiveCount;    // Number of primitives (for leaves)
    int depth;             // Node depth for debugging
    int padding[3];        // Alignment
};
```

## 3. BVH Construction Algorithm

### 3.1 Top-Down Construction Strategy

Use SAH (Surface Area Heuristic) based construction:

```cpp
class BeamBVHBuilder
{
public:
    struct BuildParams
    {
        int maxDepth = 20;
        int maxPrimitivesPerLeaf = 4;
        float traversalCost = 1.0f;
        float intersectionCost = 2.0f;
    };

    std::vector<BeamBVHNode> build(
        const std::vector<BeamData>& beams,
        const std::vector<BallData>& balls,
        const BuildParams& params = {});

private:
    struct BuildContext
    {
        std::vector<int> primitiveIndices;
        std::vector<BoundingBox> primitiveBounds;
        BoundingBox centroidBounds;
    };

    int buildRecursive(
        BuildContext& context,
        int start, int end,
        int depth,
        std::vector<BeamBVHNode>& nodes);
        
    int findBestSplit(
        const BuildContext& context,
        int start, int end,
        int& splitAxis, float& splitPos);
        
    float evaluateSAH(
        const BuildContext& context,
        int start, int split, int end,
        int axis, float pos);
};
```

### 3.2 Bounding Box Calculation

```cpp
BoundingBox calculateBeamBounds(const BeamData& beam)
{
    // Calculate tight bounding box for conical beam
    float maxRadius = std::max(beam.startRadius, beam.endRadius);
    
    BoundingBox bounds;
    bounds.min = float4{
        std::min(beam.startPos.x, beam.endPos.x) - maxRadius,
        std::min(beam.startPos.y, beam.endPos.y) - maxRadius,
        std::min(beam.startPos.z, beam.endPos.z) - maxRadius,
        0.0f
    };
    bounds.max = float4{
        std::max(beam.startPos.x, beam.endPos.x) + maxRadius,
        std::max(beam.startPos.y, beam.endPos.y) + maxRadius,
        std::max(beam.startPos.z, beam.endPos.z) + maxRadius,
        0.0f
    };
    
    return bounds;
}

BoundingBox calculateBallBounds(const BallData& ball)
{
    BoundingBox bounds;
    bounds.min = float4{
        ball.position.x - ball.radius,
        ball.position.y - ball.radius,
        ball.position.z - ball.radius,
        0.0f
    };
    bounds.max = float4{
        ball.position.x + ball.radius,
        ball.position.y + ball.radius,
        ball.position.z + ball.radius,
        0.0f
    };
    
    return bounds;
}
```

## 4. Distance Field Evaluation

### 4.1 OpenCL Kernel Functions

```cl
// Distance to conical beam (cylinder with different start/end radii)
float beamDistance(float3 pos, global const BeamData* beam)
{
    float3 start = beam->startPos.xyz;
    float3 end = beam->endPos.xyz;
    float3 axis = end - start;
    float length = length(axis);
    
    if (length < 1e-6f) {
        // Degenerate beam - treat as sphere
        float radius = max(beam->startRadius, beam->endRadius);
        return length(pos - start) - radius;
    }
    
    axis /= length;
    
    // Project point onto beam axis
    float3 toPoint = pos - start;
    float t = clamp(dot(toPoint, axis), 0.0f, length);
    
    // Interpolate radius at projection point
    float radius = mix(beam->startRadius, beam->endRadius, t / length);
    
    // Calculate distance to axis
    float3 projection = start + t * axis;
    float distToAxis = length(pos - projection);
    
    // Distance to cylindrical surface
    float surfaceDist = distToAxis - radius;
    
    // Handle caps based on cap style
    if (t <= 0.0f) {
        // Near start cap
        switch (beam->startCapStyle) {
            case 0: // hemisphere
                return length(pos - start) - beam->startRadius;
            case 1: // sphere  
                return length(pos - start) - beam->startRadius;
            case 2: // butt
                return max(surfaceDist, -(t));
        }
    } else if (t >= length) {
        // Near end cap
        switch (beam->endCapStyle) {
            case 0: // hemisphere
                return length(pos - end) - beam->endRadius;
            case 1: // sphere
                return length(pos - end) - beam->endRadius;
            case 2: // butt
                return max(surfaceDist, t - length);
        }
    }
    
    return surfaceDist;
}

// Distance to ball
float ballDistance(float3 pos, global const BallData* ball)
{
    return length(pos - ball->position.xyz) - ball->radius;
}

// BVH traversal for beam lattice
float beamLatticeDistance(float3 pos, int rootIndex, PAYLOAD_ARGS)
{
    struct PrimitiveMeta root = primitives[rootIndex];
    
    // Early exit if outside root bounding box
    float boundsDist = bbBox(pos, root.boundingBox.min.xyz, root.boundingBox.max.xyz);
    if (boundsDist > 10.0f) {
        return boundsDist;
    }
    
    float minDist = FLT_MAX;
    int stack[32];
    int stackPtr = 0;
    
    // Push root node
    stack[stackPtr++] = root.start;
    
    while (stackPtr > 0) {
        int nodeIndex = stack[--stackPtr];
        struct PrimitiveMeta node = primitives[nodeIndex];
        
        // Check if point is near bounding box
        float nodeBoundsDist = bbBox(pos, node.boundingBox.min.xyz, node.boundingBox.max.xyz);
        if (nodeBoundsDist > minDist) {
            continue; // Skip this subtree
        }
        
        if (node.primitiveType == SDF_BEAM_BVH_NODE) {
            // Internal node - add children to stack
            if (node.start >= 0) stack[stackPtr++] = node.start;  // left child
            if (node.end >= 0) stack[stackPtr++] = node.end;      // right child
        } else {
            // Leaf node - test primitives
            for (int i = node.start; i < node.start + node.end; i++) {
                if (primitives[i].primitiveType == SDF_BEAM) {
                    global const BeamData* beam = (global const BeamData*)&data[primitives[i].start];
                    float dist = beamDistance(pos, beam);
                    minDist = min(minDist, dist);
                } else if (primitives[i].primitiveType == SDF_BALL) {
                    global const BallData* ball = (global const BallData*)&data[primitives[i].start];
                    float dist = ballDistance(pos, ball);
                    minDist = min(minDist, dist);
                }
            }
        }
    }
    
    return minDist;
}
```

## 5. 3MF Integration

### 5.1 Extend Importer3mf.cpp

```cpp
void Importer3mf::processBeamLattice(
    Lib3MF::PBeamLattice beamLattice, 
    Document& doc, 
    ResourceKey const& key)
{
    std::vector<BeamData> beams;
    std::vector<BallData> balls;
    
    // Extract beam data
    auto beamIterator = beamLattice->GetBeams();
    while (beamIterator->MoveNext()) {
        auto beam = beamIterator->GetCurrent();
        
        BeamData beamData;
        
        // Get beam indices and positions
        Lib3MF_uint32 startIndex, endIndex;
        beam->GetIndices(startIndex, endIndex);
        
        auto startVertex = beamLattice->GetVertex(startIndex);
        auto endVertex = beamLattice->GetVertex(endIndex);
        
        beamData.startPos = float4{
            startVertex.m_Coordinates[0],
            startVertex.m_Coordinates[1], 
            startVertex.m_Coordinates[2],
            0.0f
        };
        beamData.endPos = float4{
            endVertex.m_Coordinates[0],
            endVertex.m_Coordinates[1],
            endVertex.m_Coordinates[2], 
            0.0f
        };
        
        // Get radii
        double startRadius, endRadius;
        beam->GetRadii(startRadius, endRadius);
        beamData.startRadius = static_cast<float>(startRadius);
        beamData.endRadius = static_cast<float>(endRadius);
        
        // Get cap styles
        Lib3MF::eBeamLatticeCapMode startCap, endCap;
        beam->GetCapModes(startCap, endCap);
        beamData.startCapStyle = static_cast<int>(startCap);
        beamData.endCapStyle = static_cast<int>(endCap);
        
        beams.push_back(beamData);
    }
    
    // Extract ball data if present
    Lib3MF::eBeamLatticeBallMode ballMode;
    double ballRadius;
    beamLattice->GetBallOptions(ballMode, ballRadius);
    
    if (ballMode != Lib3MF::eBeamLatticeBallMode::None) {
        auto ballIterator = beamLattice->GetBalls();
        while (ballIterator->MoveNext()) {
            auto ball = ballIterator->GetCurrent();
            
            BallData ballData;
            auto vertex = beamLattice->GetVertex(ball->GetVertexIndex());
            ballData.position = float4{
                vertex.m_Coordinates[0],
                vertex.m_Coordinates[1],
                vertex.m_Coordinates[2],
                0.0f
            };
            ballData.radius = static_cast<float>(ball->GetRadius());
            
            balls.push_back(ballData);
        }
    }
    
    // Build BVH
    BeamBVHBuilder builder;
    auto bvhNodes = builder.build(beams, balls);
    
    // Add to document's primitive system
    addBeamLatticeToPrimitives(doc, key, beams, balls, bvhNodes);
}

void Importer3mf::addBeamLatticeToPrimitives(
    Document& doc,
    ResourceKey const& key,
    const std::vector<BeamData>& beams,
    const std::vector<BallData>& balls, 
    const std::vector<BeamBVHNode>& bvhNodes)
{
    // Implementation details for adding to primitive system
    // Similar to existing mesh primitive handling
}
```

### 5.2 Extend MeshObject Processing

Modify existing mesh object processing to detect and handle beam lattices:

```cpp
void Importer3mf::loadMeshIfNecessary(
    Lib3MF::PModel model,
    Lib3MF::PMeshObject meshObject, 
    Document& doc)
{
    // ... existing mesh loading code ...
    
    // Check for beam lattice
    if (meshObject->HasBeamLattice()) {
        auto beamLattice = meshObject->BeamLattice();
        processBeamLattice(beamLattice, doc, key);
    }
}
```

## 6. Implementation Phases

### Phase 1: Core Data Structures ✅ COMPLETE

- [x] Define `BeamData` and `BallData` structures in `types.h`
- [x] Extend `PrimitiveMeta` with beam-specific fields  
- [x] Add new `PrimitiveType` enum values (`SDF_BEAM_LATTICE`, `SDF_BEAM`, `SDF_BALL`, `SDF_BEAM_BVH_NODE`)
- [x] Create `BeamBVHNode` structure for hierarchical organization

### Phase 2: BVH Construction ✅ COMPLETE

- [x] Implement `BeamBVHBuilder` class with SAH-based splitting
- [x] Bounding box calculation for beams and balls
- [x] Unit tests for BVH construction with comprehensive validation
- [x] Memory-efficient data layout for GPU transfer

### Phase 3: Distance Field Kernels ✅ COMPLETE

- [x] OpenCL kernel for beam distance calculation with conical support
- [x] Ball distance calculation with sphere primitives
- [x] BVH traversal kernel with stack-based algorithm
- [x] Cap style handling (hemisphere, sphere, butt) with proper distance calculation
- [x] Test-driven development with reference implementation validation

### Phase 4: 3MF Integration and Node System (Current Phase)

This phase focuses on integrating beam lattices into the existing Gladius pipeline, following the established patterns for mesh resources and node-based operations.

#### 4.1: Resource Management Integration ✅ **COMPLETED**

**Study Current Architecture:**
- [x] Analyze existing mesh resource management in `MeshResource.h/.cpp`
- [x] Understand `ResourceManager` pattern and `IResource` interface
- [x] Study `Importer3mf` architecture for mesh processing
- [x] Examine `SignedDistanceToMesh` node implementation
- [x] Review `Builder` class for resource reference handling

**Create BeamLatticeResource:**
- [x] ✅ Implement `BeamLatticeResource` class extending `ResourceBase`
- [x] ✅ Handle beam lattice data loading and memory management
- [x] ✅ Integrate with existing `Primitives` system for GPU data transfer
- [x] ✅ Support resource dependency tracking and lifecycle management

**Status**: ✅ **COMPLETED** - BeamLatticeResource successfully implemented, compiled, and integrated

**Implementation Summary:**
- Created `src/BeamLatticeResource.h` with complete interface
- Implemented `src/BeamLatticeResource.cpp` with full functionality
- Constructor validates input and initializes BVH parameters  
- `loadImpl()` constructs BVH using existing BeamBVHBuilder
- `write()` method integrates with Primitives system
- Payload data prepared for GPU transfer in correct format
- Build system integration successful (compiles without errors)

```cpp
class BeamLatticeResource : public ResourceBase  // ✅ IMPLEMENTED
{
public:
    BeamLatticeResource(ResourceKey key, 
                       std::vector<BeamData>&& beams,
                       std::vector<BallData>&& balls);
    
    void loadImpl() override;                     // ✅ BVH construction
    void write(Primitives& primitives) override; // ✅ GPU data transfer
    
    const std::vector<BeamData>& getBeams() const { return m_beams; }
    const std::vector<BallData>& getBalls() const { return m_balls; }
    
private:
    std::vector<BeamData> m_beams;
    std::vector<BallData> m_balls;
    std::vector<BeamBVHNode> m_bvhNodes;
    PayloadData m_payloadData; // For GPU transfer
};
```

#### 4.2: 3MF Import Pipeline Extension ✅ **COMPLETED**

**Status:** ✅ Implementation complete and compiled successfully

**Completed Implementation:**
- ✅ Added `loadBeamLatticeIfNecessary()` method to handle 3MF beam lattice parsing
- ✅ Integrated with existing `loadMeshIfNecessary()` pattern
- ✅ Implemented beam lattice resource creation and registration
- ✅ Added proper error handling and logging

**Implementation Details:**
```cpp
// Successfully implemented in Importer3mf.h/.cpp
void loadBeamLatticeIfNecessary(Lib3MF::PModel model,
                               Lib3MF::PMeshObject meshObject,
                               Document& doc);

// Added ResourceManager support
void addResource(ResourceKey key, std::unique_ptr<BeamLatticeResource>&& resource);
```

- ✅ Extract beam data from 3MF structures (positions, radii, caps)
- ✅ Extract ball data from vertices according to beam lattice specification
- ✅ Create BeamLatticeResource using existing constructor (includes BVH build)
- ✅ Register resource in `ResourceManager` following established patterns

**Files Modified:**
- ✅ `src/io/3mf/Importer3mf.h` - Added method declaration
- ✅ `src/io/3mf/Importer3mf.cpp` - Implemented beam lattice import logic
- ✅ `src/ResourceManager.h` - Added beam lattice resource support
- ✅ `src/ResourceManager.cpp` - Added implementation

**Ready for Phase 4.3: Node System Integration**

---

#### 4.3: Node System Integration

**Create SignedDistanceToBeamLattice Node:**
- [ ] Implement node class following `SignedDistanceToMesh` pattern
- [ ] Add to `NodeTypes` tuple and node categories
- [ ] Handle resource reference management
- [ ] Implement `updateMemoryOffsets()` for GPU data

```cpp
class SignedDistanceToBeamLattice : public ClonableNode<SignedDistanceToBeamLattice>
{
public:
    SignedDistanceToBeamLattice();
    explicit SignedDistanceToBeamLattice(NodeId id);
    void updateMemoryOffsets(GeneratorContext& generatorContext) override;
};
```

**Node Configuration:**
- [ ] Input: `Pos` (Float3), `BeamLattice` (ResourceId)  
- [ ] Output: `Distance` (Float)
- [ ] Category: `Primitive`
- [ ] Hidden parameters: `Start`, `End` for GPU memory indexing

#### 4.4: Builder Pattern Integration

**Extend Builder Class:**
- [ ] Add `addBeamLatticeRef()` method following `addResourceRef()` pattern
- [ ] Handle coordinate system transformation
- [ ] Support union operations with existing geometry
- [ ] Integration with assembly building pipeline

```cpp
void Builder::addBeamLatticeRef(Model& target,
                               ResourceKey const& resourceKey,
                               nodes::Port& coordinateSystemPort)
{
    // Create resource node
    auto resourceNode = target.create<nodes::Resource>();
    resourceNode->parameter().at(FieldNames::ResourceId) = 
        VariantParameter(resourceKey.getResourceId().value_or(0));
    
    // Create beam lattice distance node
    auto beamLatticeNode = target.create<nodes::SignedDistanceToBeamLattice>();
    beamLatticeNode->parameter().at(FieldNames::Pos).setInputFromPort(coordinateSystemPort);
    beamLatticeNode->parameter().at(FieldNames::BeamLattice).setInputFromPort(
        resourceNode->getOutputs().at(FieldNames::Value));
    
    // Union with existing geometry
    auto& shapePort = beamLatticeNode->getOutputs().at(FieldNames::Distance);
    unionWithExistingGeometry(target, shapePort);
}
```

#### 4.5: OpenCL Integration

**Extend SDF Kernel System:**
- [ ] Add beam lattice case to `model()` function in `sdf.cl`
- [ ] Integrate existing beam lattice kernels from Phase 3
- [ ] Handle primitive type dispatch for `SDF_BEAM_LATTICE`
- [ ] Support BVH traversal in ray marching pipeline

```cl
// In sdf.cl
float beamLatticePrimitive(float3 pos, int index, PAYLOAD_ARGS)
{
    return beam_lattice_distance(pos, index, primitives, data);
}

// Add to main model() function switch statement
case SDF_BEAM_LATTICE:
    return beamLatticePrimitive(pos, index, PASS_PAYLOAD_ARGS);
```

#### 4.6: Assembly Model Integration

**Internal Assembly Graph Support:**
- [ ] Update `createObject()` in Importer3mf to handle beam lattice objects
- [ ] Support beam lattice build items in assembly composition
- [ ] Handle transformation matrices for beam lattice instances
- [ ] Integrate with existing build plate assembly logic

```cpp
void Importer3mf::createObject(Lib3MF::CObject& objectRes,
                              Lib3MF::PModel& model,
                              ResourceKey& key,
                              const nodes::Matrix4x4& trafo,
                              Document& doc)
{
    if (objectRes.IsMeshObject()) {
        auto mesh = model->GetMeshObjectByID(objectRes.GetUniqueResourceID());
        
        // Check for beam lattice
        if (mesh->HasBeamLattice()) {
            auto beamLattice = mesh->BeamLattice();
            addBeamLatticeObject(model, key, mesh, trafo, doc);
        } else {
            addMeshObject(model, key, mesh, trafo, doc);
        }
    }
    // ... existing levelset handling
}
```

#### 4.7: Field Names and Type System

**Extend Node Type System:**
- [ ] Add `BeamLattice` to FieldNames structure
- [ ] Update parameter type mappings
- [ ] Support beam lattice resource references
- [ ] Maintain type safety in node graph

```cpp
// In nodesfwd.h FieldNames
static auto constexpr BeamLattice = "beamlattice";

// Type rules for beam lattice node
TypeRule rule = {RuleType::Default,
                InputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3},
                            {FieldNames::BeamLattice, ParameterTypeIndex::ResourceId}},
                OutputTypeMap{{FieldNames::Distance, ParameterTypeIndex::Float}}};
```

#### 4.8: Testing and Validation

**Integration Tests:**
- [ ] 3MF file loading with beam lattices
- [ ] Resource management lifecycle
- [ ] Node graph construction and validation
- [ ] OpenCL kernel execution with beam lattice data
- [ ] Assembly composition with multiple beam lattices

**Test Cases:**
- [ ] Simple beam lattice (few beams)
- [ ] Complex lattice structures (hundreds of beams)
- [ ] Mixed geometry (beams + meshes + level sets)
- [ ] Clipping mesh integration
- [ ] Ball mode variations (none, mixed, all)

### Phase 5: Testing and Optimization (Week 5-6)

- [ ] Performance benchmarking against mesh-only models
- [ ] Memory usage optimization for large lattices
- [ ] Ray marching integration testing with complex scenes
- [ ] 3MF compliance validation with reference implementations
- [ ] User interface integration for beam lattice visualization

## 7. Pipeline Architecture Deep Dive

### 7.1 Current Gladius Resource Pipeline

The existing pipeline follows this flow:

```
3MF File → Importer3mf → ResourceManager → Primitives → OpenCL Kernels
     ↓           ↓             ↓              ↓            ↓
   lib3mf   Extract Data   Register      GPU Buffer   Ray Marching
            Create Mesh    Resources     Transfer     Distance Eval
```

**Key Components:**
1. **Importer3mf**: Parses 3MF, creates resources, builds node graphs
2. **ResourceManager**: Manages resource lifecycle, memory allocation
3. **Primitives**: GPU buffer management, data transfer to OpenCL
4. **Node System**: Graph-based computation, assembly composition
5. **OpenCL Kernels**: GPU distance field evaluation

### 7.2 Beam Lattice Integration Points

**Resource Management Layer:**
- `BeamLatticeResource` extends `ResourceBase`
- Integrates with existing `IResource` interface
- Handles BVH construction during resource loading
- Manages GPU memory layout for beam and ball data

**Import Layer:**
- `processBeamLattice()` follows `loadMeshIfNecessary()` pattern
- Extracts 3MF beam lattice data using lib3mf
- Creates `BeamLatticeResource` instances
- Registers with `ResourceManager` using `ResourceKey`

**Node System Layer:**
- `SignedDistanceToBeamLattice` follows `SignedDistanceToMesh` pattern
- Connects to resource via `ResourceId` parameter
- Handles coordinate system transformation
- Supports union/intersection with other geometry

**Assembly Layer:**
- Build items with beam lattices create assembly nodes
- Transformation matrices applied to beam lattice instances
- Support for multiple beam lattice objects per assembly
- Integration with existing level set and mesh objects

**OpenCL Execution Layer:**
- New `SDF_BEAM_LATTICE` primitive type
- BVH traversal using existing stack-based algorithms
- Beam distance calculation with conical/cylindrical support
- Ball distance calculation for lattice nodes

### 7.3 Data Flow Diagram

```
3MF BeamLattice Element
         ↓
    processBeamLattice()
         ↓
   Extract Beam/Ball Data
         ↓
    Build BVH Structure
         ↓
  Create BeamLatticeResource
         ↓
   Register with ResourceManager
         ↓
  Create SignedDistanceToBeamLattice Node
         ↓
    Add to Assembly Model
         ↓
   Flatten to GPU Primitives
         ↓
   Transfer to OpenCL Buffer
         ↓
   Execute beam_lattice_distance()
```

### 7.4 Memory Layout Optimization

**Host-Side Structure:**
- `BeamData[]`: Contiguous array of beam primitives
- `BallData[]`: Contiguous array of ball primitives  
- `BeamBVHNode[]`: Hierarchical indexing structure
- `PrimitiveMeta[]`: GPU primitive metadata

**GPU Buffer Layout:**
```
PrimitiveBuffer: [Meta0][Meta1][Meta2]...[MetaN]
DataBuffer:      [BVH0 ][Beam0][Ball0 ]...[DataN]
                    ↑      ↑      ↑
                   Root   Leaf   Leaf
```

**Memory Alignment:**
- All structures 16-byte aligned for optimal GPU access
- Beam/Ball data stored contiguously for cache efficiency
- BVH nodes indexed directly from primitive metadata

### 7.5 Node Graph Assembly

**Resource Reference Pattern:**
```
Resource Node (ResourceId) → SignedDistanceToBeamLattice (Pos, BeamLattice) → Distance Output
```

**Coordinate System Integration:**
```
Transformation Node → Position → BeamLattice Node → Union/Min Node → End Node
```

**Multi-Object Assembly:**
```
Mesh Resource → Mesh Distance ----↘
                                   Min → End
BeamLattice Resource → BL Distance ↗
```

### 7.6 Implementation Dependencies and Order

**Critical Path Dependencies:**

1. **Foundation** (Phase 1&2 ✅): Data structures and BVH must be complete first
2. **Kernels** (Phase 3 ✅): OpenCL implementation must be validated before integration  
3. **Resource Layer** (Phase 4.1): BeamLatticeResource must be implemented before nodes
4. **Node System** (Phase 4.3): SignedDistanceToBeamLattice depends on resource layer
5. **Import Pipeline** (Phase 4.2): Can be developed in parallel with node system
6. **Builder Integration** (Phase 4.4): Depends on both nodes and import pipeline
7. **Assembly Integration** (Phase 4.6): Final integration step

**Parallel Development Opportunities:**
- 4.1 (Resource Layer) + 4.2 (Import Pipeline) can be developed simultaneously
- 4.7 (Field Names) can be done early alongside 4.3 (Node System)
- 4.8 (Testing) should be ongoing throughout Phase 4

**File Modification Requirements:**

```cpp
// Core type system
src/kernel/types.h                    // ✅ Already extended with beam types
src/nodes/nodesfwd.h                  // Add BeamLattice field name
src/nodes/DerivedNodes.h              // Add SignedDistanceToBeamLattice class
src/nodes/DerivedNodes.cpp            // Implement beam lattice node

// Resource management  
src/BeamLatticeResource.h             // New file - beam lattice resource
src/BeamLatticeResource.cpp           // New file - implementation
src/ResourceManager.h                 // No changes needed (uses IResource interface)

// 3MF import pipeline
src/io/3mf/Importer3mf.h             // Add processBeamLattice declaration
src/io/3mf/Importer3mf.cpp           // Implement beam lattice processing

// Node system integration
src/nodes/Builder.h                   // Add addBeamLatticeRef declaration  
src/nodes/Builder.cpp                 // Implement beam lattice builder

// OpenCL kernel integration
src/kernel/sdf.cl                     // Add SDF_BEAM_LATTICE case
src/kernels/beam_lattice_kernels.cl   // ✅ Already implemented
```

**Testing Strategy per Component:**

```cpp
// Unit tests for each component
tests/BeamLatticeResourceTest.cpp     // Resource lifecycle testing
tests/BeamLatticeNodeTest.cpp         // Node behavior testing  
tests/BeamLatticeImportTest.cpp       // 3MF import testing
tests/BeamLatticeIntegrationTest.cpp  // Full pipeline testing
```

**Integration Checkpoints:**

1. **Resource Layer Checkpoint**: BeamLatticeResource can load and write to Primitives
2. **Node System Checkpoint**: SignedDistanceToBeamLattice connects properly in graphs
3. **Import Checkpoint**: 3MF files with beam lattices load without errors
4. **Builder Checkpoint**: Assembly models include beam lattice geometry
5. **Kernel Checkpoint**: OpenCL kernels execute beam lattice distance calculations
6. **End-to-End Checkpoint**: Complete 3MF → Assembly → GPU → Rendering pipeline

## 8. Performance Considerations

- Align data structures to 16-byte boundaries for efficient GPU access
- Use structure-of-arrays (SoA) layout where beneficial
- Minimize memory bandwidth through compact representations

## 8. Performance Considerations

### 8.2 BVH Quality Metrics

- Target for SAH cost reduction of 80%+ compared to linear traversal
- Maintain balanced trees (depth variance < 20%)
- Optimize for ray coherence in typical rendering scenarios

### 8.3 GPU Kernel Optimization

- Minimize divergent branching in BVH traversal
- Use local memory for stack-based traversal when possible
- Early termination strategies for distance field evaluation

## 9. Testing Strategy

### 9.1 Unit Tests

- BVH construction correctness
- Distance field accuracy against analytical solutions
- Memory layout validation
- Performance regression tests

### 9.2 Integration Tests

- 3MF file loading with beam lattices
- Rendering pipeline integration
- Clipping mesh interaction
- Material property handling

### 9.3 Validation Datasets

- Simple test cases: single beam, cross-beams, lattice cubes
- Complex structures: gyroid lattices, truss structures
- Performance datasets: 1K, 10K, 100K beam structures

## 10. Dependencies and Libraries

### 10.1 Required Dependencies

- **lib3mf**: Already available via VCPKG for 3MF file parsing
- **Eigen**: Already in use for vector/matrix operations
- **OpenCL**: Existing GPU compute infrastructure

### 10.2 Optional Enhancements

- **embree**: For high-quality BVH construction reference
- **openvdb**: For voxel-based beam lattice representations
- **tbb**: For parallel BVH construction

## 11. Future Enhancements

### 11.1 Advanced Features

- Non-circular beam cross-sections (requires extension)
- Adaptive level-of-detail for large lattices
- GPU-based BVH construction and refitting
- Temporal coherence for animated lattices

### 11.2 Integration Opportunities

- Material-aware distance fields for multi-material beams
- Procedural lattice generation tools
- Optimization algorithms for lattice structure design
- Integration with topology optimization workflows

## Conclusion

This implementation plan provides a comprehensive roadmap for adding 3MF beam lattice support to Gladius with efficient BVH-based distance field evaluation. The modular design allows for incremental implementation while maintaining compatibility with existing systems. The focus on performance and GPU compatibility ensures the solution will scale to production use cases involving complex lattice structures.
