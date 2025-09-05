/// @file beam_lattice_kernels.cl
/// @brief OpenCL kernels for beam lattice distance field evaluation
/// @author AI Assistant  
/// @date 2025

#pragma OPENCL EXTENSION cl_khr_fp64 : enable

/// @brief Beam data structure (matches host-side BeamData)
typedef struct
{
    float4 startPos;       // Start position (w component unused)
    float4 endPos;         // End position (w component unused) 
    float startRadius;     // Radius at start
    float endRadius;       // Radius at end
    int startCapStyle;     // Cap style: 0=hemisphere, 1=sphere, 2=butt
    int endCapStyle;       // Cap style for end
    int materialId;        // Material/property ID
    int padding;           // Alignment padding
} BeamData;

/// @brief Ball data structure (matches host-side BallData)
typedef struct
{
    float4 position;       // Ball center (w component unused)
    float radius;          // Ball radius
    int materialId;        // Material/property ID
    int padding[2];        // Alignment padding
} BallData;

/// @brief BVH node structure for beam lattices
typedef struct
{
    float4 boundingBoxMin;
    float4 boundingBoxMax;
    int leftChild;         // Index to left child (-1 if leaf)
    int rightChild;        // Index to right child (-1 if leaf)
    int primitiveStart;    // First primitive index (for leaves)
    int primitiveCount;    // Number of primitives (for leaves)
    int depth;             // Node depth for debugging
    int padding[3];        // Alignment
} BeamBVHNode;

/// @brief Distance to conical beam (cylinder with different start/end radii)
float beamDistance(float3 pos, __global const BeamData* beam)
{
    float3 start = beam->startPos.xyz;
    float3 end = beam->endPos.xyz;
    float3 axis = end - start;
    float length_val = length(axis);
    
    // Handle degenerate beam (zero length) - treat as sphere
    if (length_val < 1e-6f) {
        float radius = max(beam->startRadius, beam->endRadius);
        return length(pos - start) - radius;
    }
    
    axis /= length_val;
    
    // Project point onto beam axis
    float3 toPoint = pos - start;
    float t_unclamped = dot(toPoint, axis);
    float t = clamp(t_unclamped, 0.0f, length_val);
    
    // Interpolate radius at projection point
    float radius = mix(beam->startRadius, beam->endRadius, t / length_val);
    
    // Calculate distance to axis
    float3 projection = start + t * axis;
    float distToAxis = length(pos - projection);
    
    // Distance to cylindrical surface
    float surfaceDist = distToAxis - radius;
    
    // Handle caps based on cap style
    if (t_unclamped <= 0.0f) {
        // Near start cap
        switch (beam->startCapStyle) {
            case 0: // hemisphere
                return length(pos - start) - beam->startRadius;
            case 1: // sphere  
                return length(pos - start) - beam->startRadius;
            case 2: // butt
                return max(surfaceDist, -t_unclamped);
            default:
                return surfaceDist;
        }
    } else if (t_unclamped >= length_val) {
        // Near end cap
        float overrun = t_unclamped - length_val;
        switch (beam->endCapStyle) {
            case 0: // hemisphere
                return length(pos - end) - beam->endRadius;
            case 1: // sphere
                return length(pos - end) - beam->endRadius;
            case 2: // butt
                return max(surfaceDist, overrun);
            default:
                return surfaceDist;
        }
    }
    
    return surfaceDist;
}

/// @brief Distance to ball
float ballDistance(float3 pos, __global const BallData* ball)
{
    return length(pos - ball->position.xyz) - ball->radius;
}

/// @brief Bounding box distance function
float boundingBoxDistance(float3 pos, float3 boxMin, float3 boxMax)
{
    float3 center = (boxMin + boxMax) * 0.5f;
    float3 extents = (boxMax - boxMin) * 0.5f;
    float3 d = fabs(pos - center) - extents;
    return length(max(d, 0.0f)) + min(max(d.x, max(d.y, d.z)), 0.0f);
}

/// @brief Test kernel for beam distance calculation
__kernel void test_beam_distance(
    __global const float3* point,
    __global const BeamData* beam_data,
    __global float* result)
{
    *result = beamDistance(*point, beam_data);
}

/// @brief Test kernel for ball distance calculation  
__kernel void test_ball_distance(
    __global const float3* point,
    __global const BallData* ball_data,
    __global float* result)
{
    *result = ballDistance(*point, ball_data);
}

/// @brief BVH traversal for beam lattice distance field evaluation
__kernel void beam_lattice_distance(
    __global const float3* points,           // Input points to evaluate
    __global float* distances,               // Output distances
    __global const BeamBVHNode* bvhNodes,   // BVH nodes
    __global const BeamData* beamData,      // Beam primitive data
    __global const BallData* ballData,      // Ball primitive data
    __global const int* primitiveIndices,   // Primitive indices for leaves
    const int rootNodeIndex,                // Root BVH node index
    const int numBeams,                     // Number of beam primitives
    const int numBalls)                     // Number of ball primitives
{
    int gid = get_global_id(0);
    
    float3 pos = points[gid];
    float minDist = FLT_MAX;
    
    // BVH traversal stack
    int stack[32];
    int stackPtr = 0;
    
    // Push root node
    stack[stackPtr++] = rootNodeIndex;
    
    while (stackPtr > 0 && stackPtr < 32) {
        int nodeIndex = stack[--stackPtr];
        __global const BeamBVHNode* node = &bvhNodes[nodeIndex];
        
        // Check if point is near bounding box
        float nodeBoundsDist = boundingBoxDistance(pos, node->boundingBoxMin.xyz, node->boundingBoxMax.xyz);
        if (nodeBoundsDist > minDist) {
            continue; // Skip this subtree
        }
        
        if (node->leftChild >= 0 || node->rightChild >= 0) {
            // Internal node - add children to stack (if they exist)
            if (node->rightChild >= 0 && stackPtr < 31) {
                stack[stackPtr++] = node->rightChild;
            }
            if (node->leftChild >= 0 && stackPtr < 31) {
                stack[stackPtr++] = node->leftChild;
            }
        } else {
            // Leaf node - test primitives
            for (int i = 0; i < node->primitiveCount; i++) {
                int primIndex = primitiveIndices[node->primitiveStart + i];
                
                float dist;
                if (primIndex < numBeams) {
                    // Beam primitive
                    dist = beamDistance(pos, &beamData[primIndex]);
                } else {
                    // Ball primitive
                    int ballIndex = primIndex - numBeams;
                    if (ballIndex < numBalls) {
                        dist = ballDistance(pos, &ballData[ballIndex]);
                    } else {
                        continue; // Invalid index
                    }
                }
                
                minDist = min(minDist, dist);
            }
        }
    }
    
    distances[gid] = minDist;
}

/// @brief Simplified beam lattice distance for single point evaluation
float evaluateBeamLatticeDistance(
    float3 pos,
    __global const BeamBVHNode* bvhNodes,
    __global const BeamData* beamData,
    __global const BallData* ballData,
    __global const int* primitiveIndices,
    int rootNodeIndex,
    int numBeams,
    int numBalls)
{
    float minDist = FLT_MAX;
    
    // BVH traversal stack
    int stack[32];
    int stackPtr = 0;
    
    // Push root node
    stack[stackPtr++] = rootNodeIndex;
    
    while (stackPtr > 0 && stackPtr < 32) {
        int nodeIndex = stack[--stackPtr];
        __global const BeamBVHNode* node = &bvhNodes[nodeIndex];
        
        // Check if point is near bounding box
        float nodeBoundsDist = boundingBoxDistance(pos, node->boundingBoxMin.xyz, node->boundingBoxMax.xyz);
        if (nodeBoundsDist > minDist) {
            continue; // Skip this subtree
        }
        
        if (node->leftChild >= 0 || node->rightChild >= 0) {
            // Internal node - add children to stack
            if (node->rightChild >= 0 && stackPtr < 31) {
                stack[stackPtr++] = node->rightChild;
            }
            if (node->leftChild >= 0 && stackPtr < 31) {
                stack[stackPtr++] = node->leftChild;
            }
        } else {
            // Leaf node - test primitives
            for (int i = 0; i < node->primitiveCount; i++) {
                int primIndex = primitiveIndices[node->primitiveStart + i];
                
                float dist;
                if (primIndex < numBeams) {
                    // Beam primitive
                    dist = beamDistance(pos, &beamData[primIndex]);
                } else {
                    // Ball primitive
                    int ballIndex = primIndex - numBeams;
                    if (ballIndex < numBalls) {
                        dist = ballDistance(pos, &ballData[ballIndex]);
                    } else {
                        continue; // Invalid index
                    }
                }
                
                minDist = min(minDist, dist);
            }
        }
    }
    
    return minDist;
}
