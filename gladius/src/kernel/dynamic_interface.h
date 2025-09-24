#ifndef GLADIUS_DYNAMIC_IFACE_GUARD
#define GLADIUS_DYNAMIC_IFACE_GUARD
// Dynamic interface for Gladius OpenCL dynamic compilation layer.
// This header provides the essential declarations needed by dynamically generated model code.
// It's injected into the dynamic compilation unit but NOT into the static library.

// Essential function prototypes that the generated model code needs
float3 matrixVectorMul3f(float16 matrix, float3 vector);
float glsl_mod1f(float a, float b);
float bbBox(float3 pos, float3 bbmin, float3 bbmax);

// Forward declarations for types used in function signatures
struct BoundingBox;
struct PrimitiveMeta;
struct RenderingSettings;
struct Command;
enum PrimitiveType;

// Payload argument list (matches arguments.h exactly)
#define PAYLOAD_ARGS                                                                               \
    float4 buildArea, __global struct PrimitiveMeta *primitives, int primitivesSize,               \
      __global float *data, int dataSize, struct RenderingSettings renderingSettings,              \
      __read_only image3d_t preCompSdf, __global float *parameter, __global struct Command *cmds,  \
      int sizeOfCmds, struct BoundingBox preCompSdfBBox

#define PASS_PAYLOAD_ARGS                                                                          \
    buildArea, primitives, primitivesSize, data, dataSize, renderingSettings, preCompSdf,          \
      parameter, cmds, sizeOfCmds, preCompSdfBBox

#endif // GLADIUS_DYNAMIC_IFACE_GUARD
