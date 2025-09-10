
#define CONVEX_A 1u
#define CONVEX_B 2u
#define CONVEX_C 4u
#define CONVEX_AB 8u
#define CONVEX_BC 16u
#define CONVEX_CA 32u

// for easier ports of glsl functions
bool equal(float3 a, float3 b)
{
    return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}

// Vendor specific floating point modulo implementation
#define VENDOR_NVIDIA

#ifdef VENDOR_NVIDIA
// Use default floating point mod intrinsic on NV
#define modImpl fmod
#else
// Use custom implementation on non-NVIDIA (AMD/Intel)
float modImpl(float a, float b)
{
    return mod(uintBitsToFloat(floatBitsToUint(a) + 1u),
               b); // T ODO: Implement uintBitsToFloat and floatBitsToUint
}
#endif


float glsl_mod1f(float a, float b)
{
    return a - b * floor(a / b);
}

float3 glsl_mod3f(float3 a, float3 b)
{
    return a - b * floor(a / b);
}

float16 glsl_mod16f(float16 a, float16 b)
{
    return a - b * floor(a / b);
}


float wrap(float x) 
{
    float iptr;
    return fract(x, &iptr);
}

// mirror (for texture coordinates)
float mirrorRepeated(float x)
{
    x = fmod(fabs(x + (x < 0)),2);
    return x >= 1.f ? 1.f - x: x;
}

float clamp01(float x)
{
    return clamp(x, 0.f, 1.f);
}



float4 matrixVectorMul4f(float16 matrix, float4 vector)
{
    float4 row0 = (float4) (matrix.s0, matrix.s4, matrix.s8, matrix.sc);
    float4 row1 = (float4) (matrix.s1, matrix.s5, matrix.s9, matrix.sd);
    float4 row2 = (float4) (matrix.s2, matrix.s6, matrix.sa, matrix.se);
    float4 row3 = (float4) (matrix.s3, matrix.s7, matrix.sb, matrix.sf);

    float4 result =
      (float4) (dot(row0, vector), dot(row1, vector), dot(row2, vector), dot(row3, vector));
    return result;
}

float3 matrixVectorMul3f(float16 matrix, float3 vector)
{
    return matrixVectorMul4f(matrix, (float4) (vector, 1.0f)).xyz;
}

float16 transpose(float16 matrix)
{
    float16 result;
    result.s0 = matrix.s0;
    result.s1 = matrix.s4;
    result.s2 = matrix.s8;
    result.s3 = matrix.sc;
    result.s4 = matrix.s1;
    result.s5 = matrix.s5;
    result.s6 = matrix.s9;
    result.s7 = matrix.sd;
    result.s8 = matrix.s2;
    result.s9 = matrix.s6;
    result.sa = matrix.sa;
    result.sb = matrix.se;
    result.sc = matrix.s3;
    result.sd = matrix.s7;
    result.se = matrix.sb;
    result.sf = matrix.sf;
    return result;
}

float2 matrixVectorMul2f(float4 matrix, float2 vector)
{
    float2 row0 = (float2) (matrix.s0, matrix.s2);
    float2 row1 = (float2) (matrix.s1, matrix.s3);

    float2 result = (float2) (dot(row0, vector), dot(row1, vector));
    return result;
}

float infiniteCylinder(float3 pos, float radius)
{
    return length(pos.xz) - radius;
}

float cylinder(float3 pos, float radius, float height)
{
    float2 d = fabs((float2) (length(pos.xz), pos.y)) - (float2) (radius, height);
    return min(max(d.x, d.y), 0.0f) + length(max(d, 0.0f));
}

float cylinderFromTo(float3 pos, float3 start, float3 end, float radius)
{
    float3 const a = start;
    float3 const b = end;
    float3 ba = b - a;
    float3 pa = pos - a;
    float baba = dot(ba, ba);
    float paba = dot(pa, ba);
    float x = length(pa * baba - ba * paba) - radius * baba;
    float y = fabs(paba - baba * 0.5f) - baba * 0.5f;
    float x2 = x * x;
    float y2 = y * y * baba;
    float d = (max(x, y) < 0.f) ? -min(x2, y2) : (((x > 0.f) ? x2 : 0.f) + ((y > 0.f) ? y2 : 0.f));
    return sign(d) * sqrt(fabs(d)) / baba;
}

float ndot(float2 a, float2 b)
{
    return a.x * b.x - a.y * b.y;
}


float line(float2 pos, float2 start, float2 end)
{
    float2 posStart = pos - start;
    float2 endStart = end - start;

    float const l = distance(start, end);
    if (l == 0.0f)
    {
        return distance(start, pos);
    }
    float posOnLine = dot(posStart, endStart) / dot(endStart, endStart);
    float h = clamp(posOnLine, 0.0f, 1.0f);
    float det = posStart.x * endStart.y - posStart.y * endStart.x;
    float const signess = (fabs(det) > 0.0f) ? sign(det) : 1.0f;
    return length(posStart - endStart * h) * -signess;
}

float sqDistance(float2 start, float2 end)
{
    float2 const vec = end - start;
    return dot(vec, vec);
}

float sqDistance3f(float3 start, float3 end)
{
    float3 const vec = end - start;
    return dot(vec, vec);
}

float sqLength(float3 vector)
{
    return dot(vector, vector);
}

float unsignedSqLine(float2 pos, float2 start, float2 end)
{
    float2 posStart = pos - start;
    float2 endStart = end - start;

    float const sqL = sqDistance(start, end);
    if (sqL == FLT_EPSILON)
    {
        return sqDistance(start, pos);
    }
    float posOnLine = dot(posStart, endStart) / dot(endStart, endStart);
    float h = clamp(posOnLine, 0.0f, 1.0f);
    return sqDistance(posStart, endStart * h);
}

float unsignedLine(float2 pos, float2 start, float2 end)
{
    return sqrt(unsignedSqLine(pos, start, end));
}

float unite(float sdfA, float sdfB)
{
    return min(sdfA, sdfB);
}

struct DistanceColor uniteColor(struct DistanceColor a, struct DistanceColor b)
{
    struct DistanceColor result;
    result.signedDistance = min(a.signedDistance, b.signedDistance);
    result.color = (a.signedDistance < b.signedDistance) ? a.color : b.color;
    result.type = (a.signedDistance < b.signedDistance) ? a.type : b.type;
    return result;
}

float4 uniteWithColor(float4 sdfWithColorA, float4 sdfWithColorB)
{
    return (float4) ((sdfWithColorA.w < sdfWithColorB.w) ? sdfWithColorA.xyz : sdfWithColorB.xyz,
                     min(sdfWithColorA.w, sdfWithColorB.w));
}

float difference(float sdfA, float sdfB)
{
    return max(sdfA, -sdfB);
}

float differenceSmooth(float sdfA, float sdfB, float smoothingIntensity)
{
    float const ratio = clamp(0.5f - 0.5f * (sdfA + sdfB) / smoothingIntensity, 0.f, 1.f);
    return mix(sdfA, -sdfB, ratio) + smoothingIntensity * ratio * (1.0f - ratio);
}

float intersection(float sdfA, float sdfB)
{
    return max(sdfA, sdfB);
}

float uniteSmooth(float sdfA, float sdfB, float smoothingIntensity)
{
    float const ratio = clamp(0.5f + 0.5f * (sdfB - sdfA) / smoothingIntensity, 0.0f, 1.0f);
    return mix(sdfB, sdfA, ratio) - smoothingIntensity * ratio * (1.0f - ratio);
}

float2 translate2f(float2 pos, float2 translation)
{
    return pos - translation;
}

float3 translate3f(float3 pos, float3 translation)
{
    return pos - translation;
}


float lines(float3 pos, __global float * points, const uint start, const uint end)
{
    float signedDistance = FLT_MAX;
    for (uint i = start; i < end; i += 2)
    {
        const float dist = line(pos.xy, points[i], points[i + 1]);
        signedDistance = (fabs(dist) - fabs(signedDistance) < 0.0f) ? dist : signedDistance;
    }
    return signedDistance;
}

float polygon(float3 pos, __global float * points, const uint start, const uint end)
{

    float sqDist = FLT_MAX;
    int windingNumber = 0;
    for (uint i = start; i <= end; ++i)
    {
        uint const next = (i == end) ? start : i + 1;
        float2 const startVertex = points[i];
        float2 const endVertex = points[next];
        float2 const startEnd = endVertex - startVertex;
        float2 const startPos = pos.xy - startVertex;

        // Using the winding number algorithm to determine the sign of the distance and
        // with that wether pos is inside the poly or outside
        // See http://geomalgorithms.com/a03-_inclusion.html for explanation
        bool const isAboveStart = pos.y >= startVertex.y;
        bool const isBelowEnd = pos.y < endVertex.y;
        bool const isLeft = startEnd.x * startPos.y - startEnd.y * startPos.x > 0;
        windingNumber = (isAboveStart && isBelowEnd && isLeft) ? windingNumber + 1 : windingNumber;
        windingNumber =
          (!isAboveStart && !isBelowEnd && !isLeft) ? windingNumber - 1 : windingNumber;

        sqDist = unite(sqDist, unsignedSqLine(pos.xy, startVertex, endVertex));
    }
    float const sign = (windingNumber == 0) ? 1.0f : -1.0f;
    return sign * sqrt(sqDist);
}

float box(float3 pos, float3 dimensions)
{
    float3 d = fabs(pos) - dimensions * 0.5f;
    return fmin(fmax(d.x, fmax(d.y, d.z)), 0.0f) + length(fmax(d, 0.0f));
}

float bbBox(float3 pos, float3 bbmin, float3 bbmax)
{
    float3 dimensions = bbmax - bbmin;
    pos = translate3f(pos, dimensions * 0.5f + bbmin);
    return box(pos, dimensions);
}

float polygonPrimitive(float3 pos, struct PrimitiveMeta primitive, global float * data)
{
    float const bb = bbBox(pos, primitive.boundingBox.min.xyz, primitive.boundingBox.max.xyz);

    if (bb > 10.1f)
    {
        return bb;
    }

    float sqDist = FLT_MAX;
    int windingNumber = 0;
    int start = primitive.start;
    int end = primitive.end;
    for (int i = start; i <= end; i += 2)
    {
        int const next = (i == end) ? start : i + 2;
        float2 const startVertex = (float2) (data[i], data[i + 1]);
        float2 const endVertex = (float2) (data[next], data[next + 1]);
        float2 const startEnd = endVertex - startVertex;
        float2 const startPos = pos.xy - startVertex;

        // Using the winding number algorithm to determine the sign of the distance and
        // with that wether pos is inside the poly or outside
        // See http://geomalgorithms.com/a03-_inclusion.html for explanation
        bool const isAboveStart = pos.y >= startVertex.y;
        bool const isBelowEnd = pos.y < endVertex.y;
        bool const isLeft = startEnd.x * startPos.y - startEnd.y * startPos.x > 0;
        windingNumber = (isAboveStart && isBelowEnd && isLeft) ? windingNumber + 1 : windingNumber;
        windingNumber =
          (!isAboveStart && !isBelowEnd && !isLeft) ? windingNumber - 1 : windingNumber;

        sqDist = unite(sqDist, unsignedSqLine(pos.xy, startVertex, endVertex));
    }
    float const sign = (windingNumber == 0) ? 1.0f : -1.0f;
    return sign * sqrt(sqDist);
}

float3 cartesianToBarycentric(float3 pos, float3 v1, float3 v2, float3 v3)
{
    float const det = (v2.y - v3.y) * (v1.x - v3.x) + (v3.x - v2.x) * (v1.y - v3.y);

    float3 lambda = (float3) (0.f);
    bool isValidDet = fabs(det) > 0.f;
    lambda.x =
      (isValidDet) ? ((v2.y - v3.y) * (pos.x - v3.x) + (v3.x - v2.x) * (pos.y - v3.y)) / det : 0.f;
    lambda.y =
      (isValidDet) ? ((v3.y - v1.y) * (pos.x - v3.x) + (v1.x - v3.x) * (pos.y - v3.y)) / det : 0.f;
    lambda.z = 1.0 - lambda.x - lambda.y;
    return lambda;
}

float3 barycentricToCartesian(float3 barycentricPos, float3 v1, float3 v2, float3 v3)
{
    return barycentricPos.x * v1 + barycentricPos.y * v2 + barycentricPos.z * v3;
}

float signF(float value)
{
    return value >= 0.f ? 1.f : -1.f;
}

float sqTriangle(float3 pos, int startId, global float * data)
{
    float3 const a = (float3) (data[startId], data[startId + 1], data[startId + 2]);
    float3 const b = (float3) (data[startId + 3], data[startId + 4], data[startId + 5]);
    float3 const c = (float3) (data[startId + 6], data[startId + 7], data[startId + 8]);

    // Inspired by the method discribed in "Real-Time Collision Detection, Ericson, 2004"
    float3 const ab = b - a;
    float3 const ac = c - a;
    float3 const ap = pos - a;

    float const d1 = dot(ab, ap);
    float const d2 = dot(ac, ap);

    float3 const bp = pos - b;
    float const d3 = dot(ab, bp);
    float const d4 = dot(ac, bp);

    float const vc = d1 * d4 - d3 * d2;

    float3 const cp = pos - c;
    float const d5 = dot(ab, cp);
    float const d6 = dot(ac, cp);

    float const vb = d5 * d2 - d1 * d6;
    float const va = d3 * d6 - d5 * d4;

    float3 const faceNormal = normalize(cross(ab, ac));

    // Check if pos is in vertex region outside A
    if (d1 <= 0.f && d2 <= 0.f)
    {
        return sqDistance3f(pos, a); // barycentric coordinates(1, 0, 0)
    }

    // Check if pos is in vertex region outside B
    if (d3 >= 0.f && d4 <= d3)
    {
        return sqDistance3f(pos, b); // barycentric coordinates (0, 1, 0)
    }

    // Check if pos is in edge region of AB
    if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        float const v = d1 / (d1 - d3);
        float3 const nearestPoint = a + v * ab; // barycentric coordinates (1-v, v, 0)
        return sqDistance3f(pos, nearestPoint);
    }

    // Check if pos is in vertex region outside C
    if (d6 >= 0.f && d5 <= d6)
    {
        return sqDistance3f(pos, c); // barycentric coordinates (0, 0, 1)
    }

    // Check if pos is in the edge region of AC -> projection on AC
    if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        float const w = d2 / (d2 - d6);
        float3 const nearestPoint = a + w * ac; // barycentric coordinates (1-w, 0, w)
        return sqDistance3f(pos, nearestPoint);
    }

    // Check if pos is in the edge region of BC -> projection on BC
    if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        float const w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        float3 const nearestPoint = b + w * (c - b); // barycentric coordinates (0, 1-w, w)
        return sqDistance3f(pos, nearestPoint);
    }

    // The projection of pos is inside the triangle
    float const denom = 1.f / (va + vb + vc);
    float const v = vb * denom;
    float const w = vc * denom;
    float3 const nearestPointInPlane = a + ab * v + ac * w;

    return sqDistance3f(pos, nearestPointInPlane);
}

float meshSegment(float3 pos, float sqDist, struct PrimitiveMeta primitive, global float * data)
{
    int const start = primitive.start;
    int const end = primitive.end;
    for (int i = start; i <= end; i += 13)
    {
        float const distTri = sqTriangle(pos, i, data);
        sqDist = (fabs(distTri) < fabs(sqDist)) ? distTri : sqDist;
    }
    return sqDist;
}

void push(__private int * stack, int value)
{
    int const maxDepth = 64;
    // stack[0] is the index of the top element
    stack[0] = min(maxDepth, stack[0] + 1);
    int const top = stack[0];
    stack[top] = value;
}

int pop(__private int * stack)
{
    int const top = stack[0];
    int const valueAtTop = stack[top];
    stack[top] = 0;
    stack[0] = max(0, stack[0] - 1);
    return valueAtTop;
}
bool empty(__private int * stack)
{
    return stack[0] < 1;
}

float meshNode(float3 pos,
               int nodeIndex,
               __global struct PrimitiveMeta * primitives,
               global float * data)
{
    struct PrimitiveMeta node = primitives[nodeIndex];
    float const bb = bbBox(pos, node.boundingBox.min.xyz, node.boundingBox.max.xyz);
    if (bb > 10.0f)
    {
        return bb;
    }

    __private int stack[100];
    for (int i = 0; i < 100; ++i)
    {
        stack[i] = node.start;
    }

    float sqDist = FLT_MAX;
    float outsideLeafBB = 1.f;
    for (;;)
    {
        struct PrimitiveMeta node = primitives[nodeIndex];
        float const bb = bbBox(pos, node.boundingBox.min.xyz, node.boundingBox.max.xyz);
        if (bb <= 10.0f)
        {
            if (node.primitiveType == SDF_MESH_TRIANGLES)
            {
                sqDist = meshSegment(pos, sqDist, node, data);
            }
            else
            {
                push(stack, node.start); // left
                push(stack, node.end);   // right
            }
        }
        else
        {
            sqDist = min(bb * bb, sqDist);
        }
        if (empty(stack))
        {
            break;
        }
        nodeIndex = pop(stack);
    }
    return sqrt(fabs(sqDist));
}

// Mesh only
float meshPrimitive(float3 pos, int index, PAYLOAD_ARGS)
{
    return meshNode(pos, index, primitives, data);
}

float vdbModel(float3 pos, int index, PAYLOAD_ARGS)
{
    struct PrimitiveMeta node = primitives[index];
    // float const boundingBox = bbBox(pos, node.boundingBox.min.xyz, node.boundingBox.max.xyz);
    // float const bandwidth = 50.f;
    // if (boundingBox < bandwidth)
    {

        // Trilinear interpolation, see https://en.wikipedia.org/wiki/Trilinear_interpolation
        cnanovdb_readaccessor acc;

        global cnanovdb_griddata * grid = (global cnanovdb_griddata *) (&data[node.start]);
        cnanovdb_readaccessor_init(&acc, cnanovdb_treedata_rootF(cnanovdb_griddata_tree(grid)));

        float3 posVoxel = pos * node.scaling;
        int3 coordVoxel = convert_int3(floor(posVoxel));

        cnanovdb_coord coordCenter;
        coordCenter.mVec[0] = coordVoxel.x;
        coordCenter.mVec[1] = coordVoxel.y;
        coordCenter.mVec[2] = coordVoxel.z;

        float3 relPos = (posVoxel - floor(posVoxel));
        cnanovdb_coord coords[8];
        coords[0] = coordCenter; // 000

        coords[1] = coordCenter; // 100
        coords[1].mVec[0] += 1;

        coords[2] = coordCenter; // 110
        coords[2].mVec[0] += 1;
        coords[2].mVec[1] += 1;

        coords[3] = coordCenter; // 010
        coords[3].mVec[1] += 1;

        coords[4] = coordCenter; // 001
        coords[4].mVec[2] += 1;

        coords[5] = coordCenter; // 101
        coords[5].mVec[0] += 1;
        coords[5].mVec[2] += 1;

        coords[6] = coordCenter; // 111
        coords[6].mVec[0] += 1;
        coords[6].mVec[1] += 1;
        coords[6].mVec[2] += 1;

        coords[7] = coordCenter; // 011
        coords[7].mVec[1] += 1;
        coords[7].mVec[2] += 1;

        float c[8];

        int const zero = min((int) (fabs(pos.x)), 0); // trick the compiler to avoid inlining
        // __attribute__((opencl_unroll_hint(1))) // compile error on some platforms
        for (int i = zero; i < 8; ++i)
        {
            c[i] = cnanovdb_readaccessor_getValueF((cnanovdb_readaccessor *) &acc, &coords[i]);
        }

        float const c00 = mix(c[0], c[1], relPos.x);
        float const c01 = mix(c[4], c[5], relPos.x);
        float const c10 = mix(c[3], c[2], relPos.x);
        float const c11 = mix(c[7], c[6], relPos.x);

        float const c0 = mix(c00, c10, relPos.y);
        float const c1 = mix(c01, c11, relPos.y);

        return mix(c0, c1, relPos.z) / node.scaling;
    }
    // return boundingBox + bandwidth;
}

float vdbModelSimple(float3 pos, int index, PAYLOAD_ARGS)
{
    struct PrimitiveMeta node = primitives[index];
    // float const boundingBox = bbBox(pos, node.boundingBox.min.xyz, node.boundingBox.max.xyz);
    // float const bandwidth = 50.f;
    // if (boundingBox < bandwidth)
    {
        struct PrimitiveMeta node = primitives[index];
        cnanovdb_readaccessor acc;

        global cnanovdb_griddata * grid = (global cnanovdb_griddata *) (&data[node.start]);
        cnanovdb_readaccessor_init(&acc, cnanovdb_treedata_rootF(cnanovdb_griddata_tree(grid)));
        pos *= node.scaling;
        cnanovdb_coord coordCenter;
        
        int3 coord = convert_int3(pos);
        coordCenter.mVec[0] = coord.x;
        coordCenter.mVec[1] = coord.y;
        coordCenter.mVec[2] = coord.z;

        return cnanovdb_readaccessor_getValueF((cnanovdb_readaccessor *) &acc, &coordCenter);
    }
    // return boundingBox + bandwidth;
}

float vdbValue(int3 coord, int index, PAYLOAD_ARGS)
{
    struct PrimitiveMeta node = primitives[index];
    cnanovdb_readaccessor acc;

    global cnanovdb_griddata * grid = (global cnanovdb_griddata *) (&data[node.start]);
    cnanovdb_readaccessor_init(&acc, cnanovdb_treedata_rootF(cnanovdb_griddata_tree(grid)));
    cnanovdb_coord coordCenter;
    coordCenter.mVec[0] = coord.x;
    coordCenter.mVec[1] = coord.y;
    coordCenter.mVec[2] = coord.z;

    return cnanovdb_readaccessor_getValueF((cnanovdb_readaccessor *) &acc, &coordCenter);
}

int faceIndexFromGrid(float3 pos, int primitiveIndex, PAYLOAD_ARGS)
{
    struct PrimitiveMeta node = primitives[primitiveIndex];

    cnanovdb_readaccessor acc;
    global cnanovdb_griddata * grid = (global cnanovdb_griddata *) (&data[node.start]);
    cnanovdb_readaccessor_init(&acc, cnanovdb_treedata_rootI32(cnanovdb_griddata_tree(grid)));
    pos *= node.scaling;
    cnanovdb_coord coordCenter;
    int3 coord = convert_int3(pos);
    coordCenter.mVec[0] = coord.x;
    coordCenter.mVec[1] = coord.y;
    coordCenter.mVec[2] = coord.z;

    return cnanovdb_readaccessor_getValueI32((cnanovdb_readaccessor *) &acc, &coordCenter);
}

float meshDist(float3 pos, int meshIndex, int faceIndex, PAYLOAD_ARGS)
{
    struct PrimitiveMeta mesh = primitives[meshIndex];
    int startIndex = mesh.start + faceIndex * 9;
    if (faceIndex < 0)
    {
        return FLT_MAX;
    }
    if (startIndex > mesh.end - 1)
    {
        return FLT_MAX;
    }
    return sqrt(sqTriangle(pos, startIndex, data));
}

float closestFaceDist(float3 pos,
                      int meshIndex,
                      int faceIndexGridIndex,
                      int * faceStartIndices,
                      PAYLOAD_ARGS)
{
    struct PrimitiveMeta grid = primitives[faceIndexGridIndex];
    struct PrimitiveMeta mesh = primitives[meshIndex];
    float const voxelSize = 1.0f / grid.scaling;
    float minDist = FLT_MAX;
    int i = 0;

    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            for (int z = -1; z <= 1; ++z)
            {
                float3 const stencilPos =
                  pos + (float3) (x * voxelSize, y * voxelSize, z * voxelSize);
                int const faceIndex =
                  faceIndexFromGrid(stencilPos, faceIndexGridIndex, PASS_PAYLOAD_ARGS);
                int const startIndex = mesh.start + faceIndex * 9;
                if (startIndex < 0 || startIndex + 9 > mesh.end)
                {
                    faceStartIndices[i] = -1;
                    i++;
                    continue;
                }
                faceStartIndices[i] = startIndex;
                ++i;
                float const sqDist = sqTriangle(pos, startIndex, data);
                minDist = min(minDist, sqDist);
            }
    return sqrt(minDist);
}

// #define TRI_EPSILON 1E-6f
#define TRI_EPSILON 0.f
bool faceBetweenCenterOfVoxelAndPos(float3 pos,
                                    float3 voxelCenter,
                                    int meshIndex,
                                    int faceStartIndex,
                                    PAYLOAD_ARGS)
{
    struct PrimitiveMeta mesh = primitives[meshIndex];
    if (faceStartIndex < 0 || faceStartIndex + 9 > mesh.end)
    {
        return false;
    }

    float3 const a = (float3) (data[faceStartIndex],
                               data[faceStartIndex + 1],
                               data[faceStartIndex + 2]); // point on the plane
    float3 const b =
      (float3) (data[faceStartIndex + 3], data[faceStartIndex + 4], data[faceStartIndex + 5]);
    float3 const c =
      (float3) (data[faceStartIndex + 6], data[faceStartIndex + 7], data[faceStartIndex + 8]);

    float3 const faceNormal = normalize(cross(a - b, a - c));

    // Inserting two points on the same side of the plane in the plane equation would result in
    // equal signs
    if (sign(dot(faceNormal, voxelCenter - a)) == sign(dot(faceNormal, pos - a)))
    {
        return false;
    }

    // check if the intersection is inside the triangle

    float3 const ab = b - a;
    float3 const ac = c - a;

    float3 const direction = normalize(voxelCenter - pos);

    float3 const pVec = cross(direction, ac);

    float const det = dot(ab, pVec);

    if (fabs(det) < FLT_EPSILON)
    {
        return false;
    }

    float3 const tVec = voxelCenter - a;

    float const invDet = 1.f / det;
    float const u = dot(tVec, pVec) * invDet;

    if (u < -TRI_EPSILON || u > 1.f + TRI_EPSILON)
    {
        return false;
    }

    float3 const qVec = cross(tVec, ab);

    float const v = dot(direction, qVec) * invDet;

    if ((v < -TRI_EPSILON) || (u + v >= 1.f + TRI_EPSILON))
    {
        return false;
    }
    return true;
}

bool meshIntersectionBetweenVoxelAndPos(float3 pos,
                                        float3 voxelCenter,
                                        int meshIndex,
                                        int * faceStartIndices,
                                        PAYLOAD_ARGS)
{
    for (int i = 0; i < 27; ++i)
    {
        if (faceBetweenCenterOfVoxelAndPos(
              pos, voxelCenter, meshIndex, faceStartIndices[i], PASS_PAYLOAD_ARGS))
        {
            return true;
        }
    }
    return false;
}

float4 getValue(int3 pos, int3 dimensions, global float * data, int start, int end)
{
    int const index = start + (pos.x + pos.y * dimensions.x + pos.z * dimensions.x * dimensions.y) * 4;
    int const clampedIndex = clamp(index, start, end - 4);
    return (float4) (data[clampedIndex], data[clampedIndex + 1], data[clampedIndex + 2], data[clampedIndex + 3]);
}

// enum TextureTileStyle
// {
//     TTS_REPEAT = 0,
//     TTS_MIRROR = 1,
//     TTS_CLAMP  = 2,
//     TTS_NONE = 3
// };

float3 applyTilesStyle(float3 pos, int3 tileStyle)
{ 
    float3 result = pos;
    result.x = (tileStyle.x == 0) ?
     wrap(pos.x) : (tileStyle.x == 1) ?
      mirrorRepeated(pos.x) : (tileStyle.x == 2) ?
       clamp01(pos.x) : pos.x;
    result.y = (tileStyle.y == 0) ?
     wrap(pos.y) : (tileStyle.y == 1) ?
      mirrorRepeated(pos.y) : (tileStyle.y == 2) ?
       clamp01(pos.y) : pos.y;
    result.z = (tileStyle.z == 0) ?
     wrap(pos.z) : (tileStyle.z == 1) ?
      mirrorRepeated(pos.z) : (tileStyle.z == 2) ?
       clamp01(pos.z) : pos.z;
    return result;
    
}

float4 sampleImageNearest4f(float3 uvw, float3 dimensions, int start, int3 tileStyle, PAYLOAD_ARGS)
{
    float4 color = (float4) (0.0f);
    if (start < 0)
    {
        return color;
    }
 
    struct PrimitiveMeta img = primitives[start];

    // compute texel coordinates
    float3 const uvwMapped = applyTilesStyle(uvw, tileStyle);
    int3 texelCoord = convert_int3(uvwMapped * dimensions);
    int3 dim = convert_int3(dimensions);
    color = getValue(texelCoord, dim, data, img.start, img.end);
    return color;
}

float4 sampleImageNearest4fvdb(float3 uvw, float3 dimensions, int start, int3 tileStyle, PAYLOAD_ARGS)
{
    float4 color = (float4) (0.0f);
    if (start < 0)
    {
        return color;
    }
 
    struct PrimitiveMeta img = primitives[start];

    // compute texel coordinates
    float3 const uvwMapped = applyTilesStyle(uvw, tileStyle)  * dimensions;
    color = (float4) (vdbModelSimple(uvwMapped, start, PASS_PAYLOAD_ARGS));
    return color;
}

float4 sampleImageLinear4fvdb(float3 uvw, float3 dimensions, int start, int3 tileStyle, PAYLOAD_ARGS)
{
    float4 color = (float4) (0.0f);
    if (start < 0)
    {
        return color;
    }
    struct PrimitiveMeta img = primitives[start];
    float3 uvwMapped = applyTilesStyle(uvw, tileStyle);
    int3 dim = convert_int3(dimensions);
    float3 texelCoord = uvwMapped * (float3) (dim.x, dim.y, dim.z);
    int3 coord = convert_int3(floor(texelCoord));  // here we truncate the float to int on purpose
    
    // trilinear interpolation
    int3 texelCoord000 = coord;
    int3 texelCoord100 = coord + (int3) (1, 0, 0);
    int3 texelCoord010 = coord + (int3) (0, 1, 0);
    int3 texelCoord110 = coord + (int3) (1, 1, 0);
    int3 texelCoord001 = coord + (int3) (0, 0, 1);
    int3 texelCoord101 = coord + (int3) (1, 0, 1);
    int3 texelCoord011 = coord + (int3) (0, 1, 1);
    int3 texelCoord111 = coord + (int3) (1, 1, 1);
    
    float3 relPos = texelCoord - floor(texelCoord);
    float c000 = vdbValue(texelCoord000, start, PASS_PAYLOAD_ARGS);
    float c100 = vdbValue(texelCoord100, start, PASS_PAYLOAD_ARGS);
    float c010 = vdbValue(texelCoord010, start, PASS_PAYLOAD_ARGS);
    float c110 = vdbValue(texelCoord110, start, PASS_PAYLOAD_ARGS);
    float c001 = vdbValue(texelCoord001, start, PASS_PAYLOAD_ARGS);
    float c101 = vdbValue(texelCoord101, start, PASS_PAYLOAD_ARGS);
    float c011 = vdbValue(texelCoord011, start, PASS_PAYLOAD_ARGS);
    float c111 = vdbValue(texelCoord111, start, PASS_PAYLOAD_ARGS);
    float c00 = mix(c000, c100, relPos.x);
    float c01 = mix(c001, c101, relPos.x);
    float c10 = mix(c010, c110, relPos.x);
    float c11 = mix(c011, c111, relPos.x);

    float4 c0 = mix(c00, c10, relPos.y);
    float4 c1 = mix(c01, c11, relPos.y);
    color = (float4) (mix(c0, c1, relPos.z));
    return color;
}

float4 sampleImageLinear4f(float3 uvw, float3 dimensions, int start, int3 tileStyle, PAYLOAD_ARGS)
{
    float4 color = (float4) (0.0f);
    if (start < 0)
    {
        return color;
    }
    struct PrimitiveMeta img = primitives[start];
    float3 uvwMapped = applyTilesStyle(uvw, tileStyle);
    int3 dim = convert_int3(dimensions);
    float3 texelCoord = uvwMapped * (float3) (dim.x, dim.y, dim.z);
    int3 coord = convert_int3(floor(texelCoord));  // here we truncate the float to int on purpose
    
    // trilinear interpolation
    int3 texelCoord000 = coord;
    int3 texelCoord100 = coord + (int3) (1, 0, 0);
    int3 texelCoord010 = coord + (int3) (0, 1, 0);
    int3 texelCoord110 = coord + (int3) (1, 1, 0);
    int3 texelCoord001 = coord + (int3) (0, 0, 1);
    int3 texelCoord101 = coord + (int3) (1, 0, 1);
    int3 texelCoord011 = coord + (int3) (0, 1, 1);
    int3 texelCoord111 = coord + (int3) (1, 1, 1);
    
    float3 relPos = texelCoord - floor(texelCoord);
    float4 c000 = getValue(texelCoord000, dim, data, img.start, img.end);
    float4 c100 = getValue(texelCoord100, dim, data, img.start, img.end);
    float4 c010 = getValue(texelCoord010, dim, data, img.start, img.end);
    float4 c110 = getValue(texelCoord110, dim, data, img.start, img.end);
    float4 c001 = getValue(texelCoord001, dim, data, img.start, img.end);
    float4 c101 = getValue(texelCoord101, dim, data, img.start, img.end);
    float4 c011 = getValue(texelCoord011, dim, data, img.start, img.end);
    float4 c111 = getValue(texelCoord111, dim, data, img.start, img.end);
    float4 c00 = mix(c000, c100, relPos.x);
    float4 c01 = mix(c001, c101, relPos.x);
    float4 c10 = mix(c010, c110, relPos.x);
    float4 c11 = mix(c011, c111, relPos.x);

    float4 c0 = mix(c00, c10, relPos.y);
    float4 c1 = mix(c01, c11, relPos.y);
    color = mix(c0, c1, relPos.z);
    return color;
}


/// @brief Distance to ball
float ballDistance(float3 pos, __global const struct BallData* ball)
{
    return length(pos - ball->positionRadius.xyz) - ball->positionRadius.w;
}

/// @brief Distance to beam (conical cylinder with caps) - GPU optimized
/// @param pos World position to evaluate
/// @param startPos Beam start position
/// @param endPos Beam end position
/// @param startRadius Radius at start of beam
/// @param endRadius Radius at end of beam
/// @param startCapStyle Cap style at start (0=hemisphere, 1=sphere, 2=butt)
/// @param endCapStyle Cap style at end (0=hemisphere, 1=sphere, 2=butt)
/// @return Signed distance to beam surface
float sdToBeam(float3 pos, 
               float3 startPos, 
               float3 endPos, 
               float startRadius, 
               float endRadius, 
               int startCapStyle, 
               int endCapStyle)
{
    float3 axis = endPos - startPos;
    float lengthSq = dot(axis, axis);
    float length_val = sqrt(lengthSq);
    
    // Handle degenerate beam (zero length) - treat as sphere
    if (length_val < 1e-6f) {
        float radius = fmax(startRadius, endRadius);
        return length(pos - startPos) - radius;
    }
    
    // Normalize axis (reuse length calculation)
    float invLength = 1.0f / length_val;
    axis *= invLength;
    
    // Project point onto beam axis
    float3 toPoint = pos - startPos;
    float t_unclamped = dot(toPoint, axis);
    float t = clamp(t_unclamped, 0.0f, length_val);
    
    // Interpolate radius at projection point
    float alpha = t * invLength;
    float radius = fma(endRadius - startRadius, alpha, startRadius);  // mix optimized as FMA
    
    // Calculate distance to axis (optimized vector ops)
    float3 projection = fma(axis, t, startPos);  // startPos + t * axis
    float3 toAxisVec = pos - projection;
    float distToAxis = length(toAxisVec);
    
    // Distance to cylindrical surface
    float surfaceDist = distToAxis - radius;
    
    // Reduce branch divergence by using conditional selection instead of switches
    // Handle caps based on cap style
    float startCapDist = FLT_MAX;
    float endCapDist = FLT_MAX;
    
    // Calculate cap distances conditionally (reduces branching)
    bool nearStart = (t_unclamped <= 0.0f);
    bool nearEnd = (t_unclamped >= length_val);
    
    if (nearStart) {
        float distToStart = length(pos - startPos);
        // Combine hemisphere/sphere cases (both use same calculation)
        float sphereDist = distToStart - startRadius;
        float buttDist = fmax(surfaceDist, -t_unclamped);
        startCapDist = (startCapStyle == 2) ? buttDist : sphereDist;
        return startCapDist;
    }
    
    if (nearEnd) {
        float distToEnd = length(pos - endPos);
        float overrun = t_unclamped - length_val;
        // Combine hemisphere/sphere cases (both use same calculation)
        float sphereDist = distToEnd - endRadius;
        float buttDist = fmax(surfaceDist, overrun);
        endCapDist = (endCapStyle == 2) ? buttDist : sphereDist;
        return endCapDist;
    }
    
    return surfaceDist;
}


/// @brief Evaluate beam lattice distance using clean BVH traversal 
/// @details Simplified and optimized BVH traversal implementation
float evaluateBeamLatticeBVH(
    float3 pos,
    int latticeIndex,
    int primitiveIndicesIndex, 
    int beamIndex,
    int ballIndex,
    PAYLOAD_ARGS)
{
    struct PrimitiveMeta lattice = primitives[latticeIndex];
    
    // Compute number of BVH nodes from data size (10 floats per node)
    int latticeDataSize = lattice.end - lattice.start;
    int numBVHNodes = latticeDataSize / 10;
    
    if (numBVHNodes <= 0) {
        return FLT_MAX; // No BVH data
    }
    
    // Get primitive indices mapping and beam/ball data
    struct PrimitiveMeta primitiveIndicesMeta = primitives[primitiveIndicesIndex];
    struct PrimitiveMeta beamPrimitive = primitives[beamIndex];
    struct PrimitiveMeta ballPrimitive = primitives[ballIndex];
    
    float minDist = FLT_MAX;
    
    // BVH traversal using proper tree structure
    int stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = 0; // Start with root node (index 0)
    
    while (stackPtr > 0 && stackPtr < 32) {
        int nodeIndex = stack[--stackPtr];
        
        if (nodeIndex >= numBVHNodes || nodeIndex < 0) {
            continue; // Skip invalid nodes
        }
        
        // Read BVH node data (10 floats per node)
        int nodeDataStart = lattice.start + nodeIndex * 10;
        
        // Bounding box (6 floats: min.xyz, max.xyz)
        float3 bbMin = (float3)(data[nodeDataStart], data[nodeDataStart + 1], data[nodeDataStart + 2]);
        float3 bbMax = (float3)(data[nodeDataStart + 3], data[nodeDataStart + 4], data[nodeDataStart + 5]);
        
        // Check if point is potentially close to bounding box  
        float bbDist = bbBox(pos, bbMin, bbMax);
        if (bbDist > minDist) {
            continue; // Skip if already found closer primitive
        }
        
        // Node metadata (4 floats: leftChild, rightChild, primitiveStart, primitiveCount)
        int leftChild = (int)data[nodeDataStart + 6];
        int rightChild = (int)data[nodeDataStart + 7]; 
        int primitiveStart = (int)data[nodeDataStart + 8];
        int primitiveCount = (int)data[nodeDataStart + 9];
        
        // Check if this is a leaf node (-1 means no child)
        bool isLeaf = (leftChild == -1 && rightChild == -1);
        
        if (isLeaf) {
            // Process primitives in this leaf node
            for (int i = 0; i < primitiveCount; ++i) {
                int primitiveDataIndex = primitiveIndicesMeta.start + (primitiveStart + i) * 3;
                
                if (primitiveDataIndex + 2 >= primitiveIndicesMeta.end) {
                    break; // Safety check for bounds
                }
                
                // Read primitive index entry (type, index, unused)
                int primitiveType = (int)data[primitiveDataIndex];
                int primitiveIndex = (int)data[primitiveDataIndex + 1];
                
                float dist = FLT_MAX;
                
                if (primitiveType == 0) { // BEAM primitive
                    int beamDataStart = beamPrimitive.start + primitiveIndex * 11; // 11 floats per beam
                    if (beamDataStart + 10 < beamPrimitive.end) {
                        // Calculate beam distance directly from flat storage
                        float3 startPos = (float3)(data[beamDataStart], data[beamDataStart + 1], data[beamDataStart + 2]);
                        float3 endPos = (float3)(data[beamDataStart + 3], data[beamDataStart + 4], data[beamDataStart + 5]);
                        float startRadius = data[beamDataStart + 6];
                        float endRadius = data[beamDataStart + 7];
                        int startCapStyle = (int)data[beamDataStart + 8];
                        int endCapStyle = (int)data[beamDataStart + 9];
                        
                        // Use extracted beam distance function
                        dist = sdToBeam(pos, startPos, endPos, startRadius, endRadius, startCapStyle, endCapStyle);
                    }
                } else if (primitiveType == 1) { // BALL primitive
                    int ballDataStart = ballPrimitive.start + primitiveIndex * 4; // 4 floats per BallData struct
                    if (ballDataStart + 3 < ballPrimitive.end) {
                        // Access ball data using optimized BallData struct layout
                        __global const struct BallData* ball = (__global const struct BallData*)&data[ballDataStart];
                        
                        dist = length(pos - ball->positionRadius.xyz) - ball->positionRadius.w;
                    }
                }
                
                minDist = min(minDist, dist);
            }
        } else {
            // Internal node - add children to stack for traversal
            // Add right child first so left is processed first (depth-first)
            if (rightChild >= 0 && rightChild < numBVHNodes && stackPtr < 31) {
                stack[stackPtr++] = rightChild;
            }
            if (leftChild >= 0 && leftChild < numBVHNodes && stackPtr < 31) {
                stack[stackPtr++] = leftChild;
            }
        }
    }
    
    return minDist;
}

/// @brief Sample primitive index from voxel grid
/// @details Helper function to sample 3D voxel grid data
uint primitiveIndexFromVoxelGrid(float3 pos, int voxelGridIndex, PAYLOAD_ARGS)
{
    struct PrimitiveMeta voxelGrid = primitives[voxelGridIndex];
    
    // Get voxel grid parameters from data
    int dataStart = voxelGrid.start;
    if (dataStart + 9 >= voxelGrid.end) {
        return 0; // Invalid voxel grid
    }
    
    // Voxel grid header: origin (3), dimensions (3), voxel size (1), max distance (1), total voxels (1)
    float3 origin = (float3)(data[dataStart], data[dataStart + 1], data[dataStart + 2]);
    int3 dimensions = (int3)((int)data[dataStart + 3], (int)data[dataStart + 4], (int)data[dataStart + 5]);
    float voxelSize = data[dataStart + 6];
    int voxelDataStart = dataStart + 9;
    
    // Convert position to voxel coordinates
    float3 localPos = (pos - origin) / voxelSize;
    int3 voxelCoord = (int3)((int)floor(localPos.x), (int)floor(localPos.y), (int)floor(localPos.z));
    
    // Check bounds
    if (voxelCoord.x < 0 || voxelCoord.x >= dimensions.x ||
        voxelCoord.y < 0 || voxelCoord.y >= dimensions.y ||
        voxelCoord.z < 0 || voxelCoord.z >= dimensions.z) {
        return 0; // Outside grid
    }
    
    // Calculate linear voxel index
    int voxelIndex = voxelCoord.z * dimensions.y * dimensions.x + voxelCoord.y * dimensions.x + voxelCoord.x;
    int dataIndex = voxelDataStart + voxelIndex;
    
    if (dataIndex >= voxelGrid.end) {
        return 0; // Out of bounds
    }
    
    return (uint)data[dataIndex];
}

/// @brief GPU-optimized helper to load beam data with coalesced memory access
/// @param beamDataStart Starting index in data array
/// @param data Global memory array
/// @param startPos Output: beam start position
/// @param endPos Output: beam end position  
/// @param startRadius Output: start radius
/// @param endRadius Output: end radius
/// @param startCapStyle Output: start cap style
/// @param endCapStyle Output: end cap style
/// @return true if data was loaded successfully
inline bool loadBeamData(int beamDataStart, __global const float* data, int dataEnd,
                        float3* startPos, float3* endPos, 
                        float* startRadius, float* endRadius,
                        int* startCapStyle, int* endCapStyle)
{
    if (beamDataStart + 10 >= dataEnd) {
        return false;
    }
    
    // Use vector loads for better memory coalescing where possible
    // Load position data as vectors when alignment allows
    __global const float* beamData = &data[beamDataStart];
    
    *startPos = (float3)(beamData[0], beamData[1], beamData[2]);
    *endPos = (float3)(beamData[3], beamData[4], beamData[5]);
    *startRadius = beamData[6];
    *endRadius = beamData[7];
    *startCapStyle = (int)beamData[8];
    *endCapStyle = (int)beamData[9];
    
    return true;
}

/// @brief Extract primitive type from encoded index (if encoding is used)
/// @details If voxel grid uses type encoding, extract type from upper bits
inline uint primitiveTypeFromVoxelGrid(uint encodedIndex)
{
    // If encoding is used, type is in upper bit (bit 31)
    return (encodedIndex >> 31) & 1u; // 0 = beam, 1 = ball
}

/// @brief GPU-optimized helper to evaluate a single primitive
/// @param pos Position to evaluate
/// @param primitiveIndex Index of primitive
/// @param primitiveType Type of primitive (0=beam, 1=ball)
/// @param beamPrimitive Beam primitive metadata
/// @param ballPrimitive Ball primitive metadata
/// @param data Global data array
/// @return Distance to primitive
inline float evaluateSinglePrimitive(float3 pos, uint primitiveIndex, uint primitiveType,
                                    struct PrimitiveMeta beamPrimitive,
                                    struct PrimitiveMeta ballPrimitive,
                                    __global const float* data)
{
    if (primitiveType == 0) { // BEAM primitive
        int beamDataStart = beamPrimitive.start + primitiveIndex * 11;
        
        float3 startPos, endPos;
        float startRadius, endRadius;
        int startCapStyle, endCapStyle;
        
        if (loadBeamData(beamDataStart, data, beamPrimitive.end,
                        &startPos, &endPos, &startRadius, &endRadius,
                        &startCapStyle, &endCapStyle)) {
            return sdToBeam(pos, startPos, endPos, startRadius, endRadius, startCapStyle, endCapStyle);
        }
    } else { // BALL primitive (primitiveType == 1)
        int ballDataStart = ballPrimitive.start + primitiveIndex * 4; // 4 floats per BallData struct
        if (ballDataStart + 3 < ballPrimitive.end) {
            __global const struct BallData* ball = (__global const struct BallData*)&data[ballDataStart];
            return length(pos - ball->positionRadius.xyz) - ball->positionRadius.w;
        }
    }
    return FLT_MAX;
}

/// @brief Evaluate beam lattice distance using voxel acceleration - GPU optimized
/// @details Uses pre-computed voxel grid to find candidate primitives efficiently
float evaluateBeamLatticeVoxel(
    float3 pos,
    int latticeIndex,
    int voxelGridIndex,
    int beamIndex,
    int ballIndex,
    PAYLOAD_ARGS)
{
    struct PrimitiveMeta beamPrimitive = primitives[beamIndex];
    struct PrimitiveMeta ballPrimitive = primitives[ballIndex];
    
    float minDist = FLT_MAX;
    
    // Get voxel grid parameters
    struct PrimitiveMeta voxelGrid = primitives[voxelGridIndex];
    float voxelSize = data[voxelGrid.start + 6]; // Voxel size from header
    
    // Calculate voxel diagonal distance (sqrt(3) * voxelSize) - precompute for efficiency
    float voxelDiagonal = 1.732050808f * voxelSize; // sqrt(3) ≈ 1.732050808
    
    // First, check distance at current position only
    uint encodedIndex = primitiveIndexFromVoxelGrid(pos, voxelGridIndex, PASS_PAYLOAD_ARGS);
    
    if (encodedIndex != 0) {
        // Extract primitive index and type (combine bit operations)
        uint primitiveIndex = encodedIndex & 0x7FFFFFFF; // Lower 31 bits
        uint primitiveType = encodedIndex >> 31; // Upper bit (optimized vs function call)
        
        float dist = evaluateSinglePrimitive(pos, primitiveIndex, primitiveType, 
                                           beamPrimitive, ballPrimitive, data);
        minDist = fmin(minDist, dist);
    }
    
    // Early exit if we're far from any primitives (GPU-friendly branching)
    // Use squared comparison to avoid sqrt in fabs
    float minDistSq = minDist * minDist;
    float voxelDiagonalSq = voxelDiagonal * voxelDiagonal;
    
    if (minDistSq > voxelDiagonalSq) {
        return minDist;
    }
    
    // Optimized neighborhood sampling with reduced branching
    // Unroll the loop partially for better GPU utilization
    // Check neighbors in order of likely importance (closer first)
    
    // Define neighbor offsets in a more cache-friendly order
    const int3 neighborOffsets[26] = {
        // Face neighbors (6) - most likely to be important
        (int3)(-1, 0, 0), (int3)(1, 0, 0), (int3)(0, -1, 0), 
        (int3)(0, 1, 0), (int3)(0, 0, -1), (int3)(0, 0, 1),
        // Edge neighbors (12)  
        (int3)(-1, -1, 0), (int3)(-1, 1, 0), (int3)(1, -1, 0), (int3)(1, 1, 0),
        (int3)(-1, 0, -1), (int3)(-1, 0, 1), (int3)(1, 0, -1), (int3)(1, 0, 1),
        (int3)(0, -1, -1), (int3)(0, -1, 1), (int3)(0, 1, -1), (int3)(0, 1, 1),
        // Corner neighbors (8)
        (int3)(-1, -1, -1), (int3)(-1, -1, 1), (int3)(-1, 1, -1), (int3)(-1, 1, 1),
        (int3)(1, -1, -1), (int3)(1, -1, 1), (int3)(1, 1, -1), (int3)(1, 1, 1)
    };
    
    // Process neighbors with early exit potential
    #pragma unroll 8  // Partial unroll for GPU optimization
    for (int i = 0; i < 26; ++i) {
        int3 offset = neighborOffsets[i];
        float3 samplePos = pos + convert_float3(offset) * voxelSize;
        uint neighborEncodedIndex = primitiveIndexFromVoxelGrid(samplePos, voxelGridIndex, PASS_PAYLOAD_ARGS);
        if (neighborEncodedIndex == encodedIndex) {
            continue; // Skip if same as already evaluated
        }
        
        if (neighborEncodedIndex != 0) {
            // Extract primitive index and type (optimized bit operations)
            uint neighborPrimitiveIndex = neighborEncodedIndex & 0x7FFFFFFF;
            uint neighborPrimitiveType = neighborEncodedIndex >> 31;
            
            float dist = evaluateSinglePrimitive(pos, neighborPrimitiveIndex, neighborPrimitiveType,
                                               beamPrimitive, ballPrimitive, data);
            
            minDist = fmin(minDist, dist);
            
            // Early exit if we're very close (GPU-friendly condition)
            // This reduces unnecessary computation for nearby surfaces
            if (dist < voxelSize * 0.1f) {
                break; // Found very close primitive, no need to check others
            }
        }
    }
    
    return minDist;
}

float payloadPrimitives(float3 pos, bool useApproximation, PAYLOAD_ARGS)
{
    return payload(pos, 0, primitivesSize, PASS_PAYLOAD_ARGS);
}

__attribute__((noinline)) float payload(float3 pos, int startIndex, int endIndex, PAYLOAD_ARGS)
{
    float sdf = FLT_MAX;
    for (int i = startIndex; i < endIndex; ++i)
    {
        struct PrimitiveMeta primitive = primitives[i];
        if (primitive.primitiveType == SDF_OUTER_POLYGON)
        {
            sdf = uniteSmooth(sdf, polygonPrimitive(pos, primitive, data), 0.0f);
        }

        if (primitive.primitiveType == SDF_INNER_POLYGON)
        {
            sdf = difference(sdf, polygonPrimitive(pos, primitive, data));
        }

        if (primitive.primitiveType == SDF_MESH_TRIANGLES)
        {
#ifdef ENABLE_VDB
            int const meshIndex = i;
            int const sdfIndex = i + 1;
            int const faceIndexFarIndex = i + 2;
            int const faceIndexNearIndex = i + 3;

            int faceIndicesFar[27];
            float const distFar =
              closestFaceDist(pos, meshIndex, faceIndexFarIndex, faceIndicesFar, PASS_PAYLOAD_ARGS);
            
            if (renderingSettings.approximation & AM_DISABLE_INTERPOLATION)
            {
                sdf = vdbModelSimple(pos, sdfIndex, PASS_PAYLOAD_ARGS); //MQ works better without interpolation
            }
            else
            {
                sdf = vdbModel(pos, sdfIndex, PASS_PAYLOAD_ARGS);
            }
            
            float signess = sign(sdf);

            if (distFar > 20.0f)
            {
                return distFar * signess;
            }

            if (fabs(distFar) > 0.5f)
            {
                int faceIndices[27];
                float const distNear = closestFaceDist(
                  pos, meshIndex, faceIndexNearIndex, faceIndices, PASS_PAYLOAD_ARGS);

                return min(distNear, distFar) * signess;
            }
            return sdf;
#endif
        }

        if (primitive.primitiveType == SDF_BEAM_LATTICE)
        {
            // Find associated primitive indices, beam, and ball primitives
            int primitiveIndicesIndex = -1;
            int voxelGridIndex = -1;
            int beamIndex = -1;
            int ballIndex = -1;
            
            // Look for voxel grid acceleration first
            if (i + 1 < endIndex && primitives[i + 1].primitiveType == SDF_BEAM_LATTICE_VOXEL_INDEX) {
                voxelGridIndex = i + 1;
            }
            
            // Look for primitive indices (usually next after beam lattice or voxel grid)
            int nextIndex = (voxelGridIndex >= 0) ? voxelGridIndex + 1 : i + 1;
            if (nextIndex < endIndex && primitives[nextIndex].primitiveType == SDF_PRIMITIVE_INDICES) {
                primitiveIndicesIndex = nextIndex;
            }
            
            // Look for beam primitive (usually after primitive indices)
            nextIndex = (primitiveIndicesIndex >= 0) ? primitiveIndicesIndex + 1 : nextIndex + 1;
            if (nextIndex < endIndex && primitives[nextIndex].primitiveType == SDF_BEAM) {
                beamIndex = nextIndex;
            }
            
            // Look for ball primitive (usually after beam)
            nextIndex = beamIndex + 1;
            if (nextIndex < endIndex && primitives[nextIndex].primitiveType == SDF_BALL) {
                ballIndex = nextIndex;
            }
            
            float dist = FLT_MAX;
            
            // Choose evaluation method based on available acceleration structures
            if (voxelGridIndex >= 0 && beamIndex >= 0) {
                // Use voxel acceleration if available
                dist = evaluateBeamLatticeVoxel(pos, i, voxelGridIndex, beamIndex, ballIndex, PASS_PAYLOAD_ARGS);
            } else if (primitiveIndicesIndex >= 0) {
                // Fall back to BVH traversal
                dist = evaluateBeamLatticeBVH(pos, i, primitiveIndicesIndex, beamIndex, ballIndex, PASS_PAYLOAD_ARGS);
            }
            
            if (dist != FLT_MAX) {
                sdf = uniteSmooth(sdf, dist, 0.0f);
            }
            
            // Skip processed primitives to avoid double processing
            if (voxelGridIndex >= 0) i = voxelGridIndex;
            if (primitiveIndicesIndex >= 0) i = primitiveIndicesIndex;
            if (beamIndex >= 0) i = beamIndex;
            if (ballIndex >= 0) i = ballIndex;
        }
    }
    return sdf;
}

float buildPlattform(float3 pos)
{
    float const width = 400.0f;
    float const depth = 400.0f;
    float const height = 20.0f;

    pos = translate3f(pos, (float3) (0.5f * width, 0.5 * depth, 0.f));
    // pos = mirrorX(-pos);
    pos.x = fabs(pos.x);
    pos.y = fabs(pos.y);
    float3 const posBelowBuildPlane =
      translate3f(pos, (float3) (0.25f * width, 0.25 * depth, -height * 0.5));
    float platform = box(posBelowBuildPlane, (float3) (0.5 * width, 0.5 * depth, height));

    float3 posMounting =
      translate3f(pos.xzy, (float3) (0.5 * width - 6.f, 0.f, 0.5 * depth - 18.0f));
    float const mountingHole = infiniteCylinder(posMounting, 4.2f);
    float const lowering = cylinder(posMounting, 7.0f, height * 0.5f);
    float const loweringBox =
      box(translate3f(posMounting, (float3) (7.0f, 0.0f, 0.0f)), (float3) (14.0f, height, 14.0f));
    platform = difference(platform, mountingHole);
    platform = difference(platform, lowering);
    platform = difference(platform, loweringBox);
    return platform;
}

float buildPlattformM290(float3 pos)
{
    float const width = 250.0f;
    float const depth = 250.0f;
    float const height = 20.0f;

    pos = translate3f(pos, (float3) (0.5f * width, 0.5 * depth, 0.f));
    // pos = mirrorX(-pos);
    pos.x = fabs(pos.x);
    pos.y = fabs(pos.y);
    float3 const posBelowBuildPlane =
      translate3f(pos, (float3) (0.25f * width, 0.25 * depth, -height * 0.5));
    float platform = box(posBelowBuildPlane, (float3) (0.5 * width, 0.5 * depth, height));

    float3 posMounting =
      translate3f(pos.xzy, (float3) (0.5 * width - 6.f, 0.f, 0.5 * depth - 18.0f));
    float const mountingHole = infiniteCylinder(posMounting, 4.2f);
    float const lowering = cylinder(posMounting, 7.0f, height * 0.5f);
    float const loweringBox =
      box(translate3f(posMounting, (float3) (7.0f, 0.0f, 0.0f)), (float3) (14.0f, height, 14.0f));
    platform = difference(platform, mountingHole);
    platform = difference(platform, lowering);
    platform = difference(platform, loweringBox);
    return platform;
}

/// returns positive angles for clock wise rotation [0, PI]
float angleCW(float2 vec1, float2 vec2)
{
    vec1 = normalize(vec1);
    vec2 = normalize(vec2);

    if (length(vec1 + vec2) < 1.0E-3f) // vec1 and vec2 are directed to opposite directions
    {
        return M_PI;
    }
    float const det = vec1.x * vec2.y - vec1.y * vec2.x;
    return atan2(det, dot(vec1, vec2));
}

struct LineDistance directedLine(float2 pos, float2 start, float2 end)
{
    float2 posStart = pos - start;
    float2 endStart = end - start;

    float const l = distance(start, end);
    struct LineDistance result;
    if (l == 0.0f)
    {
        result.distance = distance(start, pos);
        return result;
    }
    float posOnLine = dot(posStart, endStart) / dot(endStart, endStart);
    float h = clamp(posOnLine, 0.0f, 1.0f);
    float det = posStart.x * endStart.y - posStart.y * endStart.x;
    // float const signess = (fabs(det) > 0 && h == posOnLine) ? sign(det) : 1.0;   //avoid
    // nasty artefacts, sadly it only works for convex polygons
    float const signess = (fabs(det) > 0.0f) ? sign(det) : 1.0f;

    result.distance = length(posStart - endStart * h) * -signess;
    result.normal = cross((float4) (end, 0.0f, 0.0f), (float4) (start, 0.0f, 0.0f)).xy;
    return result;
}

struct LineDistance uniteLine(struct LineDistance a, struct LineDistance b)
{
    float angle = angleCW(a.normal, b.normal);
    if (angle > 0.0f)
    {
        return (fabs(a.distance) > fabs(b.distance)) ? a : b;
    }
    else
    {
        return (fabs(a.distance) < fabs(b.distance)) ? a : b;
    }
}

/// To polar coordinates
float3 toPolarCoordinates(float3 pos)
{
    float3 normalizedPos = normalize(pos);
    float3 polar;
    polar.y = atan2(normalizedPos.x, normalizedPos.y);
    polar.x = acos(normalizedPos.z);
    polar.z = length(pos);
    return polar;
}



/// Calculates the inverse of a 4x4 matrix represented by a float16, based on https://stackoverflow.com/a/44446912

// float 16 component access
// s0 s1 s2 s3
// s4 s5 s6 s7
// s8 s9 sa sb
// sc sd se sf
 float16 inverse(float16 matrixToBeInverted) 
 {
    // Calculation of the matrix of minors and cofactors
    float A2323 = matrixToBeInverted.s2 * matrixToBeInverted.s7 - matrixToBeInverted.s3 * matrixToBeInverted.s6;
    float A1323 = matrixToBeInverted.s1 * matrixToBeInverted.s7 - matrixToBeInverted.s3 * matrixToBeInverted.s5;
    float A1223 = matrixToBeInverted.s1 * matrixToBeInverted.s6 - matrixToBeInverted.s2 * matrixToBeInverted.s5;
    float A0323 = matrixToBeInverted.s0 * matrixToBeInverted.s7 - matrixToBeInverted.s3 * matrixToBeInverted.s4;
    float A0223 = matrixToBeInverted.s0 * matrixToBeInverted.s6 - matrixToBeInverted.s2 * matrixToBeInverted.s4;
    float A0123 = matrixToBeInverted.s0 * matrixToBeInverted.s5 - matrixToBeInverted.s1 * matrixToBeInverted.s4;
    float A2313 = matrixToBeInverted.s9 * matrixToBeInverted.s7 - matrixToBeInverted.sb * matrixToBeInverted.s6;
    float A1313 = matrixToBeInverted.s8 * matrixToBeInverted.s7 - matrixToBeInverted.sb * matrixToBeInverted.s5;
    float A1213 = matrixToBeInverted.s8 * matrixToBeInverted.s6 - matrixToBeInverted.s9 * matrixToBeInverted.s5;
    float A2312 = matrixToBeInverted.s9 * matrixToBeInverted.s3 - matrixToBeInverted.sb * matrixToBeInverted.s2;
    float A1312 = matrixToBeInverted.s8 * matrixToBeInverted.s3 - matrixToBeInverted.sb * matrixToBeInverted.s1;
    float A1212 = matrixToBeInverted.s8 * matrixToBeInverted.s2 - matrixToBeInverted.s9 * matrixToBeInverted.s1;
    float A0313 = matrixToBeInverted.s0 * matrixToBeInverted.s7 - matrixToBeInverted.sb * matrixToBeInverted.s4;
    float A0213 = matrixToBeInverted.s0 * matrixToBeInverted.s6 - matrixToBeInverted.sa * matrixToBeInverted.s4;
    float A0312 = matrixToBeInverted.s0 * matrixToBeInverted.s3 - matrixToBeInverted.sb * matrixToBeInverted.s0;
    float A0212 = matrixToBeInverted.s0 * matrixToBeInverted.s2 - matrixToBeInverted.sa * matrixToBeInverted.s0;
    float A0113 = matrixToBeInverted.s0 * matrixToBeInverted.sb - matrixToBeInverted.s1 * matrixToBeInverted.sa;
    float A0112 = matrixToBeInverted.s0 * matrixToBeInverted.s9 - matrixToBeInverted.s1 * matrixToBeInverted.s8;

    // Calculation of the determinant of the input matrix
    float const det = matrixToBeInverted.s0 * ( matrixToBeInverted.s5 * A2323 - matrixToBeInverted.s6 * A1323 + matrixToBeInverted.s7 * A1223 )
        - matrixToBeInverted.s1 * ( matrixToBeInverted.s4 * A2323 - matrixToBeInverted.s6 * A0323 + matrixToBeInverted.s7 * A0223 )
        + matrixToBeInverted.s2 * ( matrixToBeInverted.s4 * A1323 - matrixToBeInverted.s5 * A0323 + matrixToBeInverted.s7 * A0123 )
        - matrixToBeInverted.s3 * ( matrixToBeInverted.s4 * A1223 - matrixToBeInverted.s5 * A0223 + matrixToBeInverted.s6 * A0123 ) ;

    // Calculation of the inverse matrix
    float16 inverseMatrix;
    inverseMatrix.s0 = det *   ( matrixToBeInverted.s5 * A2323 - matrixToBeInverted.s6 * A1323 + matrixToBeInverted.s7 * A1223 );
    inverseMatrix.s1 = det * - ( matrixToBeInverted.s1 * A2323 - matrixToBeInverted.s2 * A1323 + matrixToBeInverted.s3 * A1223 );
    inverseMatrix.s2 = det *   ( matrixToBeInverted.s1 * A2313 - matrixToBeInverted.s2 * A1313 + matrixToBeInverted.s3 * A1213 );
    inverseMatrix.s3 = det * - ( matrixToBeInverted.s1 * A2312 - matrixToBeInverted.s2 * A1312 + matrixToBeInverted.s3 * A1212 );
    inverseMatrix.s4 = det * - ( matrixToBeInverted.s4 * A2323 - matrixToBeInverted.s6 * A0323 + matrixToBeInverted.s7 * A0223 );
    inverseMatrix.s5 = det *   ( matrixToBeInverted.s0 * A2323 - matrixToBeInverted.s2 * A0323 + matrixToBeInverted.s3 * A0223 );
    inverseMatrix.s6 = det * - ( matrixToBeInverted.s0 * A2313 - matrixToBeInverted.s2 * A0313 + matrixToBeInverted.s3 * A0213 );
    inverseMatrix.s7 = det *   ( matrixToBeInverted.s0 * A2312 - matrixToBeInverted.s2 * A0312 + matrixToBeInverted.s3 * A0212 );
    inverseMatrix.s8 = det *   ( matrixToBeInverted.s4 * A1323 - matrixToBeInverted.s5 * A0323 + matrixToBeInverted.s7 * A0123 );
    inverseMatrix.s9 = det * - ( matrixToBeInverted.s0 * A1323 - matrixToBeInverted.s1 * A0323 + matrixToBeInverted.s3 * A0123 );
    inverseMatrix.sa = det *   ( matrixToBeInverted.s0 * A1313 - matrixToBeInverted.s1 * A0313 + matrixToBeInverted.s3 * A0113 );
    inverseMatrix.sb = det * - ( matrixToBeInverted.s0 * A1312 - matrixToBeInverted.s1 * A0312 + matrixToBeInverted.s3 * A0112 );
    inverseMatrix.sc = det * - ( matrixToBeInverted.s4 * A1223 - matrixToBeInverted.s5 * A0223 + matrixToBeInverted.s6 * A0123 );
    inverseMatrix.sd = det *   ( matrixToBeInverted.s0 * A1223 - matrixToBeInverted.s1 * A0223 + matrixToBeInverted.s2 * A0123 );
    inverseMatrix.se = det * - ( matrixToBeInverted.s0 * A1213 - matrixToBeInverted.s1 * A0213 + matrixToBeInverted.s2 * A0113 );
    inverseMatrix.sf = det *   ( matrixToBeInverted.s0 * A1212 - matrixToBeInverted.s1 * A0212 + matrixToBeInverted.s2 * A0112 );

     return inverseMatrix;
 }
