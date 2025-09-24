
float3 normalizedPosToBuildArea(float3 normalizedPos, float4 buildArea);

float2 normalizedPosToBuildArea2f(float2 normalizedPos, float4 buildArea);

float3 gradientDistMap(float2 pos, float2 cellSize, __read_only image2d_t distMap);

/// Moves vertices towards the iso line.
__constant float INVALID_VERTEX = FLT_MAX;

__constant float BRANCH_NODE = FLT_MAX;

void kernel renderSDFFirstLayer(write_only image2d_t fineLayer, // 0
                                const float branchThreshold,    // 1
                                PAYLOAD_ARGS,                   // 2, 3, 4
                                float z_mm);                    // 5

void kernel renderSDFLayer(write_only image2d_t fineLayer,    // 0
                           __read_only image2d_t coarseLayer, // 1
                           const float branchThreshold,       // 2
                           PAYLOAD_ARGS,                      // 3, 4, 5
                           float z);                          // 6

void kernel render(__write_only image2d_t result, __read_only image2d_t source);

void kernel jfaMapFromDistanceMap(__write_only image2d_t front,
                                  __read_only image2d_t distMap,
                                  float lowerLimit,
                                  float upperLimit);

void kernel jumpFlood(__write_only image2d_t front, __read_only image2d_t back, int stepLength);

void kernel distMapFromJfa(__read_only image2d_t jfaMap,
                           __write_only image2d_t distMap,
                           float4 buildArea);

void kernel renderDistMapFromJfaAndUniteNegative(__read_only image2d_t jfaMap,
                                                 __read_only image2d_t distMapPrevious,
                                                 __write_only image2d_t distMap,
                                                 float4 buildArea);
