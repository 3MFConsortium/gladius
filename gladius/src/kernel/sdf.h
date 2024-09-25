
bool equal(float3 a, float3 b);

float wrap(float x); 
float mirrorRepeated(float x);
float clamp01(float x);

float4 matrixVectorMul4f(float16 matrix, float4 vector);

float3 matrixVectorMul3f(float16 matrix, float3 vector);

float2 matrixVectorMul2f(float4 matrix, float2 vector);

float sqLength(float3 vector);

float infiniteCylinder(float3 pos, float radius);

float cylinder(float3 pos, float radius, float height);
float cylinderFromTo(float3 pos, float3 start, float3 end, float radius);


float line(float2 pos, float2 start, float2 end);

float sqDistance(float2 start, float2 end);

float unsignedSqLine(float2 pos, float2 start, float2 end);

float unsignedLine(float2 pos, float2 start, float2 end);


float unite(float sdfA, float sdfB);

struct DistanceColor uniteColor(struct DistanceColor a, struct DistanceColor b);
float4 uniteWithColor(float4 sdfWithColorA, float4 sdfWithColorB);

float difference(float sdfA, float sdfB);

float differenceSmooth(float sdfA, float sdfB, float smoothingIntensity);

float intersection(float sdfA, float sdfB);

float uniteSmooth(float sdfA, float sdfB, float smoothingIntensity);

float2 translate2f(float2 pos, float2 translation);

float3 translate3f(float3 pos, float3 translation);
float2 repeat2f(float2 pos, float2 cellSize);

float3 repeat3f(float3 pos, float3 cellSize);
float3 finiteRepeat3f(float3 pos, float3 cellSize, float3 domain);
float3 repeatPolar(float3 pos, float radius, float number);


float lines(float3 pos, __global float * points, const uint start, const uint end);

float polygon(float3 pos, __global float * points, const uint start, const uint end);

float box(float3 pos, float3 dimensions);

float bbBox(float3 pos, float3 bbmin, float3 bbmax);

float polygonPrimitive(float3 pos, struct PrimitiveMeta primitive, global float * data);

// primitives

float payloadPrimitives(float3 pos, bool useApproximation, PAYLOAD_ARGS);
float payload(float3 pos, int startIndex, int endIndex, PAYLOAD_ARGS);

float3 closestPointOnClyinder(float3 pos, float height, float radius);

// Library

float buildPlattform(float3 pos);

/// returns positive angles for clock wise rotation [0, PI]
float angleCW(float2 vec1, float2 vec2);

struct LineDistance directedLine(float2 pos, float2 start, float2 end);

struct LineDistance uniteLine(struct LineDistance a, struct LineDistance b);


/// Calculates the inverse of a 4x4 matrix represented by a float16.
float16 inverse(float16 matrixToBeInverted);

/// Calculates the transpose of a 4x4 matrix represented by a float16.
float16 transpose(float16 matrix);

float4 model(float3 Begin_1_cs, PAYLOAD_ARGS);