
struct RayCastResult rayCastInsideOut(float3 eyePostion, float3 rayDirection, PAYLOAD_ARGS)
{
    int const maxRaySteps = 20000;
    float const maxTravelDistance = 2000.0f;
    float minStepSize = 1.0E-3f;
    float traveledDistance = minStepSize;
    float distanceToSurface = minStepSize * 2.0f;

    struct RayCastResult result;
    bool hit = false;
    bool nearRange = false;
    for (int i = 0; i < maxRaySteps; ++i)
    {
        float3 rayPos = eyePostion + traveledDistance * rayDirection;
        distanceToSurface = -model(rayPos, PASS_PAYLOAD_ARGS).w;
        traveledDistance += (nearRange) ? max(distanceToSurface * 0.1f, minStepSize * 0.1f) : max(distanceToSurface, minStepSize); 

        if (nearRange && distanceToSurface < 0.f)
        {
            hit = true;
            break;
        }

        if (!nearRange && distanceToSurface < 0.f)
        {
            traveledDistance-= minStepSize;
            nearRange = true;
            break;
        }

        if (traveledDistance > maxTravelDistance)
        {
            break;
        }
    }

    result.hit = (hit) ? 1.0f : -1.0f;
    result.traveledDistance = round(traveledDistance * 10000.f) / 10000.f ; // round to 1/10 micro m to avoid small difference between diffrent accelerators
    return result;
}

void writeDistanceToSurface(__write_only image2d_t depthBuffer, PAYLOAD_ARGS, float3 direction)
{
    int2 const coord = {get_global_id(0), get_global_id(1)};
    float2 const imageSize = (float2) (get_image_width(depthBuffer), get_image_height(depthBuffer));

    float2 screenPos =
      (float2) ((float) coord.x / (imageSize.x - 1), (float) coord.y / (imageSize.y - 1));
    float2 pos = normalizedPosToBuildArea2f(screenPos, buildArea);

    struct RayCastResult const raycastingResult = rayCastInsideOut(
      (float3) (pos.x, pos.y, renderingSettings.z_mm), direction, PASS_PAYLOAD_ARGS);
    write_imagef(depthBuffer, coord, (float4) (raycastingResult.traveledDistance));
};

void kernel distanceToBottom(__write_only image2d_t depthBuffer, PAYLOAD_ARGS)
{
    float3 const rayDirection = (float3) (0.f, 0.f, -1.f);
    writeDistanceToSurface(depthBuffer, PASS_PAYLOAD_ARGS, rayDirection);
};

void kernel distanceToTop(__write_only image2d_t depthBuffer, PAYLOAD_ARGS)
{
    float3 const rayDirection = (float3) (0.f, 0.f, 1.f);
    writeDistanceToSurface(depthBuffer, PASS_PAYLOAD_ARGS, rayDirection);
};
