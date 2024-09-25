

void kernel resample(__write_only image2d_t result, __read_only image2d_t source);

void kernel renderScene(__write_only image2d_t result,  // 0
                        PAYLOAD_ARGS,                   // 2 - 9
                        float3 eyePosition,             // 10
                        float16 modelViewPerspectiveMat // 11
);
