#ifndef PAYLOAD_ARGS
#define PAYLOAD_ARGS                                                                               \
    float4 buildArea, __global struct PrimitiveMeta *primitives, int primitivesSize,               \
      __global float *data, int dataSize, struct RenderingSettings renderingSettings,              \
      __read_only image3d_t preCompSdf, __global float *parameter, __global struct Command *cmds,  \
      int sizeOfCmds, struct BoundingBox preCompSdfBBox
#define PASS_PAYLOAD_ARGS                                                                          \
    buildArea, primitives, primitivesSize, data, dataSize, renderingSettings, preCompSdf,          \
      parameter, cmds, sizeOfCmds, preCompSdfBBox
#endif
