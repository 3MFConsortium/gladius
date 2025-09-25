
#ifndef __OPENCL_VERSION__
#define COMPILING_FOR_HOST
#include <float.h>
#include <iterator>
#endif

#ifdef COMPILING_FOR_HOST
#include "../gpgpu.h"
typedef cl_float8 float8;
typedef cl_float4 float4;
typedef cl_float2 float2;
#endif

#ifndef _TYPES_H
#define _TYPES_H
enum PrimitiveType
{
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
    SDF_BEAM_LATTICE,             // Beam lattice root node (BVH acceleration)
    SDF_BEAM,                     // Individual beam primitive
    SDF_BALL,                     // Ball at beam vertex
    SDF_BEAM_BVH_NODE,            // BVH internal node for beams
    SDF_PRIMITIVE_INDICES,        // Primitive indices mapping for BVH traversal
    SDF_BEAM_LATTICE_VOXEL_INDEX, // Voxel grid with primitive indices
    SDF_BEAM_LATTICE_VOXEL_TYPE,  // Voxel grid with primitive types (optional)
};

enum ApproximationMode
{
    AM_FULL_MODEL = (1u << 0),
    AM_HYBRID = (1u << 1),
    AM_ONLY_PRECOMPSDF = (1u << 2),
    AM_DISABLE_INTERPOLATION = (1u << 3)
};

enum RenderingFlags
{
    RF_SHOW_BUILDPLATE = (1u << 0),
    RF_CUT_OFF_OBJECT = (1u << 1),
    RF_SHOW_FIELD = (1u << 2),
    RF_SHOW_STACK = (1u << 3),
    RF_SHOW_COORDINATE_SYSTEM = (1u << 4)
};

enum SamplingFilter
{
    SF_NEAREST = 0,
    SF_LINEAR = 1
};

enum TextureTileStyle
{
    TTS_REPEAT = 0,
    TTS_MIRROR = 1,
    TTS_CLAMP = 2,
    TTS_NONE = 3
};

#ifndef COMPILING_FOR_HOST
struct BoundingBox
{
    float4 min;
    float4 max;
};
#endif

#ifdef COMPILING_FOR_HOST
struct BoundingBox
{
    BoundingBox()
        : min{{FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX}}
        , max{{-FLT_MAX, -FLT_MAX, -FLT_MAX, -FLT_MAX}} {};

    BoundingBox(float4 min, float4 max)
        : min{min}
        , max{max} {};

    float4 min;
    float4 max;
};
#endif

struct PrimitiveMeta
{
    float4 center;
    int start; // left for kd-Node
    int end;   // right for kde-Node
    float scaling;
    enum PrimitiveType primitiveType;
    struct BoundingBox boundingBox;
    float4 approximationTop;
    float4 approximationBottom;
};

typedef float PrimitiveData;

// Beam data structure for lattice beams (shared between host and OpenCL)
struct BeamData
{
    float4 startPos;   // Start position (w component unused)
    float4 endPos;     // End position (w component unused)
    float startRadius; // Radius at start
    float endRadius;   // Radius at end
    int startCapStyle; // Cap style: 0=hemisphere, 1=sphere, 2=butt
    int endCapStyle;   // Cap style for end
    int materialId;    // Material/property ID
    int padding;       // Alignment padding
};

// Ball data structure for beam lattice nodes (shared between host and OpenCL)
struct BallData
{
    float4 positionRadius; // xyz = position, w = radius
};

// BVH node structure for beam lattice spatial acceleration (shared between host and OpenCL)
struct BeamBVHNode
{
    float4 boundingBoxMin;
    float4 boundingBoxMax;
    int leftChild;      // Index to left child (-1 if leaf)
    int rightChild;     // Index to right child (-1 if leaf)
    int primitiveStart; // First primitive index (for leaves)
    int primitiveCount; // Number of primitives (for leaves)
    int depth;          // Node depth for debugging
    int padding[3];     // Alignment
};

struct RenderingSettings // Note that the alignment has to be considered
{
    float time_s;
    float z_mm;

    int flags; // see enum RenderingFlags
    enum ApproximationMode approximation;

    float quality;
    float weightDistToNb;
    float weightMidPoint;
    float normalOffset;
};

struct DistanceColor
{
    float signedDistance;
    float type;
    float4 color;
};

struct LineDistance
{
    float distance;
    float2 normal;
};

struct RayCastResult
{
    float traveledDistance;
    float hit;
    float edge;
    float type;
    float4 color;
};

struct Command
{
#ifdef COMPILING_FOR_HOST
    Command()
        : type(0)
        , id(0)
        , placeholder0(0)
        , placeholder1(0)
    {
        std::fill(std::begin(args), std::begin(args) + 16, 0);
        std::fill(std::begin(output), std::begin(output) + 16, 0);
    }
#endif
    int type;
    int id;
    int placeholder0;
    int placeholder1;

    int args[32];
    int output[32];
};

enum CommandType
{
    CT_END = 0,
    CT_CONSTANT_SCALAR,
    CT_CONSTANT_VECTOR,
    CT_CONSTANT_MATRIX,
    CT_COMPOSE_VECTOR,
    CT_COMPOSE_VECTOR_FROM_SCALAR,
    CT_COMPOSE_MATRIX,
    CT_COMPOSE_MATRIX_FROM_COLUMNS,
    CT_COMPOSE_MATRIX_FROM_ROWS,
    CT_DECOMPOSE_MATRIX,
    CT_DECOMPOSE_VECTOR,
    CT_ADDITION_SCALAR,
    CT_ADDITION_VECTOR,
    CT_ADDITION_MATRIX,
    CT_MULTIPLICATION_SCALAR,
    CT_MULTIPLICATION_VECTOR,
    CT_MULTIPLICATION_MATRIX,
    CT_SUBTRACTION_SCALAR,
    CT_SUBTRACTION_VECTOR,
    CT_SUBTRACTION_MATRIX,
    CT_DIVISION_SCALAR,
    CT_DIVISION_VECTOR,
    CT_DIVISION_MATRIX,
    CT_DOT_PRODUCT,
    CT_CROSS_PRODUCT,
    CT_MATRIX_VECTOR_MULTIPLICATION,
    CT_TRANSPOSE,
    CT_INVERSE,
    CT_SINE_SCALAR,
    CT_SINE_VECTOR,
    CT_SINE_MATRIX,
    CT_COSINE_SCALAR,
    CT_COSINE_VECTOR,
    CT_COSINE_MATRIX,
    CT_SINH_SCALAR,
    CT_SINH_VECTOR,
    CT_SINH_MATRIX,
    CT_COSH_SCALAR,
    CT_COSH_VECTOR,
    CT_COSH_MATRIX,
    CT_TANH_SCALAR,
    CT_TANH_VECTOR,
    CT_TANH_MATRIX,
    CT_TANGENT_SCALAR,
    CT_TANGENT_VECTOR,
    CT_TANGENT_MATRIX,
    CT_ARC_SIN_SCALAR,
    CT_ARC_SIN_VECTOR,
    CT_ARC_SIN_MATRIX,
    CT_ARC_COS_SCALAR,
    CT_ARC_COS_VECTOR,
    CT_ARC_COS_MATRIX,
    CT_ARC_TAN_SCALAR,
    CT_ARC_TAN_VECTOR,
    CT_ARC_TAN_MATRIX,
    CT_ARC_TAN2_SCALAR,
    CT_ARC_TAN2_VECTOR,
    CT_ARC_TAN2_MATRIX,
    CT_MIN_SCALAR,
    CT_MIN_VECTOR,
    CT_MIN_MATRIX,
    CT_MAX_SCALAR,
    CT_MAX_VECTOR,
    CT_MAX_MATRIX,
    CT_ABS_SCALAR,
    CT_ABS_VECTOR,
    CT_ABS_MATRIX,
    CT_SQRT_SCALAR,
    CT_SQRT_VECTOR,
    CT_SQRT_MATRIX,
    CT_FMOD_SCALAR,
    CT_FMOD_VECTOR,
    CT_FMOD_MATRIX,
    CT_MOD_SCALAR,
    CT_MOD_VECTOR,
    CT_MOD_MATRIX,
    CT_POW_SCALAR,
    CT_POW_VECTOR,
    CT_POW_MATRIX,
    CT_EXP_SCALAR,
    CT_EXP_VECTOR,
    CT_EXP_MATRIX,
    CT_LOG_SCALAR,
    CT_LOG_VECTOR,
    CT_LOG_MATRIX,
    CT_LOG2_SCALAR,
    CT_LOG2_VECTOR,
    CT_LOG2_MATRIX,
    CT_LOG10_SCALAR,
    CT_LOG10_VECTOR,
    CT_LOG10_MATRIX,
    CT_SELECT_SCALAR,
    CT_SELECT_VECTOR,
    CT_SELECT_MATRIX,
    CT_CLAMP_SCALAR,
    CT_CLAMP_VECTOR,
    CT_CLAMP_MATRIX,
    CT_ROUND_SCALAR,
    CT_ROUND_VECTOR,
    CT_ROUND_MATRIX,
    CT_CEIL_SCALAR,
    CT_CEIL_VECTOR,
    CT_CEIL_MATRIX,
    CT_FLOOR_SCALAR,
    CT_FLOOR_VECTOR,
    CT_FLOOR_MATRIX,
    CT_SIGN_SCALAR,
    CT_SIGN_VECTOR,
    CT_SIGN_MATRIX,
    CT_FRACT_SCALAR,
    CT_FRACT_VECTOR,
    CT_FRACT_MATRIX,
    CT_SIGNED_DISTANCE_TO_MESH,
    CT_UNSIGNED_DISTANCE_TO_MESH,
    CT_LENGTH,
    CT_RESOURCE,
    CT_TRANSFORMATION,
    CT_LABEL,
    CT_MIX_SCALAR,
    CT_MIX_VECTOR,
    CT_MIX_MATRIX,
    CT_VECTOR_FROM_SCALAR,
    CT_IMAGE_SAMPLER,
    CT_BOX_MIN_MAX
};

#endif
