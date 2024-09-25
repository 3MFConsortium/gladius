
void kernel resample(__write_only image2d_t result, __read_only image2d_t source)
{
    const int2 coords = {get_global_id(0), get_global_id(1)};
    // map source into result
    int2 imageSize = (int2)(get_image_width(result), get_image_height(result));

    // Normalized position
    float2 pos =
      (float2)((float) coords.x / (imageSize.x - 1), (float) coords.y / (imageSize.y - 1));

    float4 srcValue = read_imagef(source, samplerLinearPos, pos);
    write_imagef(result, coords, srcValue);
};

void kernel renderScene(__write_only image2d_t result,  
                        PAYLOAD_ARGS,                   
                        float3 eyePosition,             
                        float16 modelViewPerspectiveMat 
)
{
    int2 const coord = {get_global_id(0), get_global_id(1)};

    int2 const imageSize = (int2)(get_image_width(result), get_image_height(result));
    float const aspectRatio =
      (float) (get_image_width(result)) / (float) (get_image_height(result));
    // Normalized position
    float2 screenPos =
      (float2)((float) coord.x / (imageSize.x - 1), 1.f - (float) coord.y / (imageSize.y - 1)) +
      (float2)(-0.5f, -0.5f);
    screenPos.y /= aspectRatio;

    // Let's shoot a ray
    float const lensLength = 1.;
    float3 const rayDirection =
      normalize(matrixVectorMul3f(modelViewPerspectiveMat, (float3)(screenPos, lensLength)));
    float const startDistance = 0.f;
    float const minDistanceForCmpStart = 1.0E-3f;
    float const sizeDecreasePerTd = 1.0E-3f;
    struct RayCastResult const raycastingResult =
      rayCast(eyePosition, rayDirection, startDistance, PASS_PAYLOAD_ARGS);

    float4 const color =
      determineColor(raycastingResult, eyePosition, rayDirection, PASS_PAYLOAD_ARGS);
    write_imagef(result, coord, (float4)(color));
};
