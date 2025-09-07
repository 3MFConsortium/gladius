# 3MF Beam Lattice Implementation Quick Start Guide

## Key Code Changes Summary

This guide provides the essential code changes needed to implement 3MF beam lattice support with BVH in Gladius.

### 1. Extend types.h

Add new primitive types and beam-specific data structures:

```cpp
// Add to PrimitiveType enum
enum PrimitiveType
{
    // ... existing types ...
    SDF_BEAM_LATTICE,      // Beam lattice root node
    SDF_BEAM,              // Individual beam primitive
    SDF_BALL,              // Ball at beam vertex
    SDF_BEAM_BVH_NODE,     // BVH internal node for beams
};

// Beam data structure (aligned for GPU)
struct BeamData
{
    float4 startPos;       // Start position (w=0)
    float4 endPos;         // End position (w=0) 
    float startRadius;     // Radius at start
    float endRadius;       // Radius at end
    int startCapStyle;     // 0=hemisphere, 1=sphere, 2=butt
    int endCapStyle;       // Cap style for end
    int materialId;        // Material/property ID
    int padding;           // Alignment
};

// Ball data structure
struct BallData
{
    float4 position;       // Ball center (w=0)
    float radius;          // Ball radius
    int materialId;        // Material/property ID
    int padding[2];        // Alignment
};
```

### 2. Create BeamBVH.h

```cpp
#pragma once
#include "types.h"
#include <vector>

namespace gladius
{
    struct BeamBVHNode
    {
        BoundingBox boundingBox;
        int leftChild;         // -1 if leaf
        int rightChild;        // -1 if leaf
        int primitiveStart;    // First primitive index
        int primitiveCount;    // Number of primitives
        int depth;
        int padding[3];
    };

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
        // Implementation details...
    };

    // Utility functions
    BoundingBox calculateBeamBounds(const BeamData& beam);
    BoundingBox calculateBallBounds(const BallData& ball);
}
```

### 3. Create BeamBVH.cpp

```cpp
#include "BeamBVH.h"
#include <algorithm>
#include <numeric>

namespace gladius
{
    BoundingBox calculateBeamBounds(const BeamData& beam)
    {
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

    std::vector<BeamBVHNode> BeamBVHBuilder::build(
        const std::vector<BeamData>& beams,
        const std::vector<BallData>& balls,
        const BuildParams& params)
    {
        std::vector<BeamBVHNode> nodes;
        // Implementation of BVH construction...
        return nodes;
    }
}
```

### 4. Add OpenCL Kernel Functions to sdf.cl

```cl
// Add to sdf.cl kernel file

float beamDistance(float3 pos, global const BeamData* beam)
{
    float3 start = beam->startPos.xyz;
    float3 end = beam->endPos.xyz;
    float3 axis = end - start;
    float length = length(axis);
    
    if (length < 1e-6f) {
        // Degenerate beam
        float radius = max(beam->startRadius, beam->endRadius);
        return length(pos - start) - radius;
    }
    
    axis /= length;
    float3 toPoint = pos - start;
    float t = clamp(dot(toPoint, axis), 0.0f, length);
    
    // Interpolate radius
    float radius = mix(beam->startRadius, beam->endRadius, t / length);
    
    // Distance to axis
    float3 projection = start + t * axis;
    float distToAxis = length(pos - projection);
    float surfaceDist = distToAxis - radius;
    
    // Handle caps based on style
    if (t <= 0.0f && beam->startCapStyle <= 1) {
        return length(pos - start) - beam->startRadius;
    } else if (t >= length && beam->endCapStyle <= 1) {
        return length(pos - end) - beam->endRadius;
    }
    
    return surfaceDist;
}

float ballDistance(float3 pos, global const BallData* ball)
{
    return length(pos - ball->position.xyz) - ball->radius;
}

float beamLatticeDistance(float3 pos, int rootIndex, PAYLOAD_ARGS)
{
    struct PrimitiveMeta root = primitives[rootIndex];
    
    float boundsDist = bbBox(pos, root.boundingBox.min.xyz, root.boundingBox.max.xyz);
    if (boundsDist > 10.0f) {
        return boundsDist;
    }
    
    float minDist = FLT_MAX;
    int stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = root.start;
    
    while (stackPtr > 0) {
        int nodeIndex = stack[--stackPtr];
        struct PrimitiveMeta node = primitives[nodeIndex];
        
        float nodeBoundsDist = bbBox(pos, node.boundingBox.min.xyz, node.boundingBox.max.xyz);
        if (nodeBoundsDist > minDist) continue;
        
        if (node.primitiveType == SDF_BEAM_BVH_NODE) {
            if (node.start >= 0) stack[stackPtr++] = node.start;
            if (node.end >= 0) stack[stackPtr++] = node.end;
        } else {
            // Leaf - test primitives
            for (int i = node.start; i < node.start + node.end; i++) {
                if (primitives[i].primitiveType == SDF_BEAM) {
                    global const BeamData* beam = (global const BeamData*)&data[primitives[i].start];
                    minDist = min(minDist, beamDistance(pos, beam));
                } else if (primitives[i].primitiveType == SDF_BALL) {
                    global const BallData* ball = (global const BallData*)&data[primitives[i].start];
                    minDist = min(minDist, ballDistance(pos, ball));
                }
            }
        }
    }
    
    return minDist;
}

// Add case to main primitive evaluation function
float primitive(float3 pos, int index, PAYLOAD_ARGS)
{
    struct PrimitiveMeta meta = primitives[index];
    
    switch (meta.primitiveType) {
        // ... existing cases ...
        case SDF_BEAM_LATTICE:
            return beamLatticeDistance(pos, index, PAYLOAD_PASS);
        case SDF_BEAM:
            {
                global const BeamData* beam = (global const BeamData*)&data[meta.start];
                return beamDistance(pos, beam);
            }
        case SDF_BALL:
            {
                global const BallData* ball = (global const BallData*)&data[meta.start];
                return ballDistance(pos, ball);
            }
        // ... other cases ...
    }
}
```

### 5. Extend Importer3mf.cpp

```cpp
// Add to Importer3mf.h
class Importer3mf 
{
    // ... existing methods ...
private:
    void processBeamLattice(Lib3MF::PBeamLattice beamLattice, 
                           Document& doc, 
                           ResourceKey const& key);
    
    void addBeamLatticeToPrimitives(Document& doc,
                                   ResourceKey const& key,
                                   const std::vector<BeamData>& beams,
                                   const std::vector<BallData>& balls,
                                   const std::vector<BeamBVHNode>& bvhNodes);
};

// Add to Importer3mf.cpp
void Importer3mf::loadMeshIfNecessary(Lib3MF::PModel model,
                                     Lib3MF::PMeshObject meshObject, 
                                     Document& doc)
{
    // ... existing mesh loading ...
    
    // Check for beam lattice
    if (meshObject->HasBeamLattice()) {
        auto beamLattice = meshObject->BeamLattice();
        processBeamLattice(beamLattice, doc, key);
    }
}

void Importer3mf::processBeamLattice(Lib3MF::PBeamLattice beamLattice, 
                                    Document& doc, 
                                    ResourceKey const& key)
{
    std::vector<BeamData> beams;
    std::vector<BallData> balls;
    
    // Extract beams
    auto beamIterator = beamLattice->GetBeams();
    while (beamIterator->MoveNext()) {
        auto beam = beamIterator->GetCurrent();
        
        BeamData beamData{};
        
        Lib3MF_uint32 startIndex, endIndex;
        beam->GetIndices(startIndex, endIndex);
        
        auto startVertex = beamLattice->GetVertex(startIndex);
        auto endVertex = beamLattice->GetVertex(endIndex);
        
        beamData.startPos = float4{
            static_cast<float>(startVertex.m_Coordinates[0]),
            static_cast<float>(startVertex.m_Coordinates[1]), 
            static_cast<float>(startVertex.m_Coordinates[2]),
            0.0f
        };
        beamData.endPos = float4{
            static_cast<float>(endVertex.m_Coordinates[0]),
            static_cast<float>(endVertex.m_Coordinates[1]),
            static_cast<float>(endVertex.m_Coordinates[2]), 
            0.0f
        };
        
        double startRadius, endRadius;
        beam->GetRadii(startRadius, endRadius);
        beamData.startRadius = static_cast<float>(startRadius);
        beamData.endRadius = static_cast<float>(endRadius);
        
        Lib3MF::eBeamLatticeCapMode startCap, endCap;
        beam->GetCapModes(startCap, endCap);
        beamData.startCapStyle = static_cast<int>(startCap);
        beamData.endCapStyle = static_cast<int>(endCap);
        
        beams.push_back(beamData);
    }
    
    // Extract balls if present
    Lib3MF::eBeamLatticeBallMode ballMode;
    double defaultBallRadius;
    beamLattice->GetBallOptions(ballMode, defaultBallRadius);
    
    if (ballMode != Lib3MF::eBeamLatticeBallMode::None) {
        auto ballIterator = beamLattice->GetBalls();
        while (ballIterator->MoveNext()) {
            auto ball = ballIterator->GetCurrent();
            
            BallData ballData{};
            auto vertex = beamLattice->GetVertex(ball->GetVertexIndex());
            ballData.position = float4{
                static_cast<float>(vertex.m_Coordinates[0]),
                static_cast<float>(vertex.m_Coordinates[1]),
                static_cast<float>(vertex.m_Coordinates[2]),
                0.0f
            };
            ballData.radius = static_cast<float>(ball->GetRadius());
            
            balls.push_back(ballData);
        }
    }
    
    // Build BVH
    BeamBVHBuilder builder;
    auto bvhNodes = builder.build(beams, balls);
    
    // Add to primitives
    addBeamLatticeToPrimitives(doc, key, beams, balls, bvhNodes);
}
```

### 6. Update CMakeLists.txt

Add the new source files:

```cmake
# Add to gladius/src/CMakeLists.txt
set(SOURCES
    # ... existing sources ...
    BeamBVH.cpp
    BeamBVH.h
)
```

## Next Steps

1. **Phase 1**: Implement the core data structures in `types.h`
2. **Phase 2**: Create the BVH builder class and test with simple cases
3. **Phase 3**: Add the OpenCL kernel functions and test distance calculations
4. **Phase 4**: Integrate with the 3MF importer and test with real files
5. **Phase 5**: Optimize performance and validate against specification

This provides the essential framework for implementing 3MF beam lattice support with BVH acceleration in Gladius.
