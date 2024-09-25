
float4 cachedSdf(float3 pos, PAYLOAD_ARGS);
float4 modelInternal(float3 pos, PAYLOAD_ARGS);
struct DistanceColor map(float3 pos, PAYLOAD_ARGS);
struct DistanceColor mapCached(float3 pos, PAYLOAD_ARGS);

struct RayCastResult
rayCast(float3 eyePostion, float3 rayDirection, float startDistance, PAYLOAD_ARGS);

float3 surfaceNormal(float3 pos, PAYLOAD_ARGS);
float calcSoftshadow(float3 pos, float3 rayDirection, float mint, float tmax, PAYLOAD_ARGS);

float calcAmbientOcclusion(float3 pos, float3 normal, PAYLOAD_ARGS);

float3 reflect(float3 inVector, float3 normal);

float4
shadingMetal(float3 pos, float3 col, float3 normalOfSurface, float3 rayDirection, PAYLOAD_ARGS);

float4 shadingWhitePolymer(float3 pos, float3 normalOfSurface, float3 rayDirection, PAYLOAD_ARGS);

float3 Uncharted2ToneMapping(float3 color);

float3 fusion(float x);
float3 distanceMeter(float dist, float rayLength, float3 rayDir, float camHeight);

float4
shadingIsolines(float3 pos, float rayLength, float3 rayDirection, float height, PAYLOAD_ARGS);

float4 shadingBluePolymer(float3 pos, float3 normalOfSurface, float3 rayDirection, PAYLOAD_ARGS);

float4 determineColor(struct RayCastResult raycastingResult,
                      float3 eyePosition,
                      float3 rayDirection,
                      PAYLOAD_ARGS);
float16 modelViewPerspectiveMatrix(float3 eyePosition, float3 lookAt, float roll);
