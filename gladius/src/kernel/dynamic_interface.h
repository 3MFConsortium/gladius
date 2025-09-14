#ifndef GLADIUS_DYNAMIC_IFACE_GUARD
#define GLADIUS_DYNAMIC_IFACE_GUARD
// Synthetic dynamic interface for Gladius OpenCL dynamic compilation layer.
// This header deliberately contains ONLY forward declarations, enums, struct layouts and
// function prototypes that the generated dynamic model sources reference directly, plus
// the PAYLOAD_ARGS / PASS_PAYLOAD_ARGS macro pair mirrored from arguments.h.
//
// Rationale:
//   The static portion of the OpenCL program (compiled into a library) already contains
//   the actual function and kernel implementations. Re-including full headers with
//   implementations for the dynamic translation unit would risk duplicate symbol definitions
//   or subtle divergence if macros expand differently. By restricting this header to a
//   stable ABI surface (types + prototypes), we enable two-level compilation/linking while
//   minimizing maintenance overhead.
//
// Maintenance guidance:
//   * If a new unresolved identifier appears during dynamic compilation, add ONLY its
//     prototype (or a minimal forward declaration) here.
//   * Keep struct field ordering and types identical to the authoritative definitions in
//     types.h (or related headers) to preserve layout compatibility on the device.
//   * Do NOT add function bodies here.
//   * Consider adding automated checks if the underlying enums/structs change frequently.
//
// NOTE: This file is embedded as a resource (cmrc) and injected ahead of generated dynamic
//       sources by CLProgram::compile().

// Forward declarations / minimal struct layouts (must match device layouts in types.h)
struct BoundingBox { float4 min; float4 max; };

enum PrimitiveType {
    SDF_OUTER_POLYGON,
    SDF_INNER_POLYGON,
    SDF_BEAMS,
    SDF_MESH_TRIANGLES,
    SDF_MESH_KD_ROOT_NODE,
    SDF_MESH_KD_NODE,
    SDF_LINES,
    SDF_VDB,
    SDF_VDB_BINARY,
    SDF_VDB_FACE_INDICES,
    SDF_VDB_GRAYSCALE_8BIT,
    SDF_IMAGESTACK,
    SDF_BEAM_LATTICE,
    SDF_BEAM,
    SDF_BALL,
    SDF_BEAM_BVH_NODE,
    SDF_PRIMITIVE_INDICES,
    SDF_BEAM_LATTICE_VOXEL_INDEX,
    SDF_BEAM_LATTICE_VOXEL_TYPE
};

struct PrimitiveMeta {
    float4 center;
    int start;
    int end;
    float scaling;
    enum PrimitiveType primitiveType;
    struct BoundingBox boundingBox;
    float4 approximationTop;
    float4 approximationBottom;
};

struct RenderingSettings {
    float time_s;
    float z_mm;
    int flags;
    int approximation;
    float quality;
    float weightDistToNb;
    float weightMidPoint;
    float normalOffset;
};

struct Command {
    int type;
    int id;
    int placeholder0;
    int placeholder1;
    int args[32];
    int output[32];
};

// Function prototypes referenced from generated model kernels
float3 matrixVectorMul3f(float16 matrix, float3 vector);
float glsl_mod1f(float a, float b);
float bbBox(float3 pos, float3 bbmin, float3 bbmax);
float payload(float3 pos, int startIndex, int endIndex,
    float4 buildArea, __global struct PrimitiveMeta *primitives, int primitivesSize,
    __global float *data, int dataSize, struct RenderingSettings renderingSettings,
    __read_only image3d_t preCompSdf, __global float *parameter, __global struct Command *cmds,
    int sizeOfCmds, struct BoundingBox preCompSdfBBox);

// Payload macros (mirrors arguments.h) - only define if not already provided
#ifndef PAYLOAD_ARGS
#define PAYLOAD_ARGS \
    float4 buildArea, __global struct PrimitiveMeta *primitives, int primitivesSize, \
      __global float *data, int dataSize, struct RenderingSettings renderingSettings, \
      __read_only image3d_t preCompSdf, __global float *parameter, __global struct Command *cmds, \
      int sizeOfCmds, struct BoundingBox preCompSdfBBox
#define PASS_PAYLOAD_ARGS \
    buildArea, primitives, primitivesSize, data, dataSize, renderingSettings, preCompSdf, \
      parameter, cmds, sizeOfCmds, preCompSdfBBox
#endif

#endif // GLADIUS_DYNAMIC_IFACE_GUARD
