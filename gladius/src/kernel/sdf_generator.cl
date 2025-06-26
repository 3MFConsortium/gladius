
#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

float3 normalizedPosToBuildArea(float3 normalizedPos, float4 buildArea)
{
    float3 const size = (float3) (buildArea.zw - buildArea.xy, 0.f);
    float3 posInBuildArea = (float3) (buildArea.xy, normalizedPos.z) + normalizedPos * size;
    return posInBuildArea;
}

float2 normalizedPosToBuildArea2f(float2 normalizedPos, float4 buildArea)
{
    float2 const size = buildArea.zw - buildArea.xy;
    float2 posInBuildArea = buildArea.xy + normalizedPos * size;
    return posInBuildArea;
}

float3 normalizedPosToBuildVolume(float3 normalizedPos, struct BoundingBox bbox)
{
    float3 const size = bbox.max.xyz - bbox.min.xyz;
    return bbox.min.xyz + normalizedPos * size;
}

float2 worldToNormalizedPos2f(float2 worldPos, float4 buildArea)
{
    float2 const size = buildArea.zw - buildArea.xy;
    float2 normalizedPos = (worldPos - buildArea.xy) / size;
    return normalizedPos;
}

float4 modelSmoothed(float3 pos, PAYLOAD_ARGS)
{
    float4 atCenter = modelInternal(pos, PASS_PAYLOAD_ARGS);
    float sd = atCenter.w;
   
    // <SMOOTHING KERNEL>

    return (float4) (atCenter.xyz, sd);
}

float modelForSlicing(float3 pos, PAYLOAD_ARGS)
{
    return modelSmoothed(pos, PASS_PAYLOAD_ARGS).w;
}

float3 gradientDistMap5Point(float2 pos, float2 cellSize, __read_only image2d_t distMap)
{
    float const offset = 1.0E-0f;
    float const sdSouth1 =
      read_imagef(distMap, samplerLinearPos, pos + (float2) (0.f, -offset) * cellSize).x;
    float const sdSouth2 =
      read_imagef(distMap, samplerLinearPos, pos + (float2) (0.f, -2.f * offset) * cellSize).x;

    float const sdNorth1 =
      read_imagef(distMap, samplerLinearPos, pos + (float2) (0.f, offset) * cellSize).x;
    float const sdNorth2 =
      read_imagef(distMap, samplerLinearPos, pos + (float2) (0.f, 2.f * offset) * cellSize).x;

    float const sdWest1 =
      read_imagef(distMap, samplerLinearPos, pos + (float2) (-offset, 0.f) * cellSize).x;
    float const sdWest2 =
      read_imagef(distMap, samplerLinearPos, pos + (float2) (-2.f * offset, 0.f) * cellSize).x;

    float const sdEast1 =
      read_imagef(distMap, samplerLinearPos, pos + (float2) (offset, 0.f) * cellSize).x;

    float const sdEast2 =
      read_imagef(distMap, samplerLinearPos, pos + (float2) (2.f * offset, 0.f) * cellSize).x;

    // five point stencil
    float const gradY =
      (-sdNorth2 + 8.f * sdNorth1 - 8.f * sdSouth1 + sdSouth2) / (12.f * cellSize.x);
    float const gradX = (-sdEast2 + 8.f * sdEast1 - 8.f * sdWest1 + sdWest2) / (12.f * cellSize.y);

    float2 grad = ((float2) (gradX, gradY));
    float magnitude = length(grad);

    return (float3) (-normalize(grad.xy), magnitude);
}

float3 gradientXY5Point(float3 pos, float3 offset, PAYLOAD_ARGS)
{
    float2 offsets[8] = {
      (float2) (0.f, -1.f),
      (float2) (0.f, -2.f),
      (float2) (0.f, 1.f),
      (float2) (0.f, 2.f),
      (float2) (-1.f, 0.f),
      (float2) (-2.f, 0.f),
      (float2) (1.f, 0.f),
      (float2) (2.f, 0.f),
    };

    float sdfValues[8];
    int const zero = min((int) (fabs(pos.x)), 0); // trick the compiler to avoid inlining
    for (int i = zero; i < 8; ++i)
    {
        sdfValues[i] = modelForSlicing(pos + (float3) (offsets[i].x, offsets[i].y, 0.f) * offset,
                                       PASS_PAYLOAD_ARGS);
    }

    float const sdSouth1 = sdfValues[0];
    float const sdSouth2 = sdfValues[1];

    float const sdNorth1 = sdfValues[2];
    float const sdNorth2 = sdfValues[3];

    float const sdWest1 = sdfValues[4];
    float const sdWest2 = sdfValues[5];

    float const sdEast1 = sdfValues[6];

    float const sdEast2 = sdfValues[7];

    // five point stencil
    float const gradY =
      (-sdNorth2 + 8.f * sdNorth1 - 8.f * sdSouth1 + sdSouth2) / (12.f * offset.x);
    float const gradX = (-sdEast2 + 8.f * sdEast1 - 8.f * sdWest1 + sdWest2) / (12.f * offset.y);

    float2 grad = ((float2) (gradX, gradY));
    float magnitude = length(grad);

    return (float3) (-normalize(grad.xy), magnitude);
}


/// Returns the value with the max absolute value multiplied with the sign of a
float maxAbsValue(float a, float b)
{
    float maxAbs = max(fabs(a), fabs(b));
    return maxAbs * sign(a);
}

// Note that z of the returned points contains the normalized position on the line
// The sign can be used to determine whether pos is in front or in the back of start
float3 closestPointOnLine(float2 pos, float2 start, float2 end)
{
    float2 const posStart = pos - start;
    float2 const endStart = end - start;

    float const sqL = dot(end - start, end - start);
    if (sqL == FLT_EPSILON)
    {
        return (float3) (start, 0.f);
    }
    float posOnLine = dot(posStart, endStart) / dot(endStart, endStart);
    return (float3) (start + endStart * posOnLine, posOnLine);
}

#define NO_INERSECTION (float2)(-FLT_MAX, -FLT_MAX)
/**
 *  \brief Returns the intersection of the line segment p1->p2 and p3->p4.
 *         If the line segments have no intersecion, NO_INERSECTION == (-FLT_MAX,-FLT_MAX) is
 * returned.
 */
float2 intersectionOfTwoLineSegments(float2 p1, float2 p2, float2 p3, float2 p4)
{
    // see https://de.wikipedia.org/w/index.php?title=Schnittpunkt&oldid=169771750
    float const determinant =
      (p1.x * (p4.y - p3.y) + p2.x * (p3.y - p4.y) + p4.x * (p2.y - p1.y) + p3.x * (p1.y - p2.y));

    if (fabs(determinant) < 0.f)
    {
        return NO_INERSECTION;
    }

    float const s =
      (p1.x * (p4.y - p3.y) + p3.x * (p1.y - p4.y) + p4.x * (p3.y - p1.y)) / determinant;
    float const t =
      -(p1.x * (p3.y - p2.y) + p2.x * (p1.y - p3.y) + p3.x * (p2.y - p1.y)) / determinant;

    if (((0.f <= s) && (s <= 1.f)) && (0.f <= t) && (t <= 1.f))
    {
        return p1 + s * (p2 - p1);
    }
    return NO_INERSECTION;
}

void kernel renderSDFFirstLayer(write_only image2d_t fineLayer, // 0
                                const float branchThreshold,    // 1
                                PAYLOAD_ARGS,                   // 2, 3, 4, 5, 6, 7, 8
                                float z_mm)                     // 8
{
    const int2 coords = {get_global_id(0), get_global_id(1)};

    int2 fineLayerSize = (int2) (get_image_width(fineLayer), get_image_height(fineLayer));

    // compute normalized position that we can use resolution indepent
    float3 pos = (float3) ((float) coords.x / (fineLayerSize.x - 1),
                           (float) coords.y / (fineLayerSize.y - 1),
                           z_mm);
    pos = normalizedPosToBuildArea(pos, buildArea);
    float sd = modelForSlicing(pos, PASS_PAYLOAD_ARGS);

    float branch = (fabs(sd) < branchThreshold) ? BRANCH_NODE : 0.f;
    write_imagef(fineLayer, coords, (float4) (sd, branch, 0.f, 0.f));
};

void kernel renderSDFLayer(write_only image2d_t fineLayer,    // 0
                           __read_only image2d_t coarseLayer, // 1
                           const float branchThreshold,       // 2
                           PAYLOAD_ARGS,                      // 3, 4, 5
                           float z)                           // 6
{
    const int2 coords = {get_global_id(0), get_global_id(1)};

    // compute normalized position that we can use resolution indepent
    int2 fineLayerSize = (int2) (get_image_width(fineLayer), get_image_height(fineLayer));

    float3 pos = (float3) ((float) coords.x / (fineLayerSize.x - 1),
                           (float) coords.y / (fineLayerSize.y - 1),
                           z);
    float4 const coarseValue = read_imagef(coarseLayer, samplerNearestPos, pos.xy);
    pos = normalizedPosToBuildArea(pos, buildArea);
    bool const discard = coarseValue.y != BRANCH_NODE;
    if (discard)
    {
        // we don't need to compute anything, we can just take the
        // value from the coarse parent layer
        write_imagef(fineLayer, coords, coarseValue);
        return;
    }
    float sd = modelForSlicing(pos, PASS_PAYLOAD_ARGS);

    float branch = (fabs(sd) < branchThreshold) ? BRANCH_NODE : 0.f;

    write_imagef(fineLayer, coords, (float4) (sd, branch, 0.f, 0.f));
};

void kernel render(__write_only image2d_t result, __read_only image2d_t source)
{
    const int2 coords = {get_global_id(0), get_global_id(1)};

    // map source into result
    int2 imageSize = (int2) (get_image_width(result), get_image_height(result));

    // Normalized position
    float2 pos =
      (float2) ((float) coords.x / (imageSize.x - 1), (float) coords.y / (imageSize.y - 1));

    float srcValue = read_imagef(source, samplerNearestPos, pos).x;
    const float minValue = -2.f;
    const float maxValue = 2.f;
    srcValue = (srcValue == FLT_MAX) ? maxValue : srcValue;
    float grayValue = (srcValue - minValue) / (maxValue - minValue);
    const float signess = (srcValue < 0.f) ? 1.f : 0.f;

    float4 color = (float4) (grayValue * signess,
                             grayValue * (1.f - signess),
                             grayValue * (1.f - signess) * 0.5f,
                             grayValue);
    color *= 0.4f;
    write_imagef(result, coords, color);
};

void kernel resample(__write_only image2d_t result, __read_only image2d_t source)
{
    const int2 coords = {get_global_id(0), get_global_id(1)};
    // map source into result
    int2 imageSize = (int2) (get_image_width(result), get_image_height(result));

    // Normalized position
    float2 pos =
      (float2) ((float) coords.x / (imageSize.x - 1), (float) coords.y / (imageSize.y - 1));

    float4 srcValue = read_imagef(source, samplerLinearPos, pos);
    write_imagef(result, coords, srcValue);
};

void kernel jfaMapFromDistanceMap(__write_only image2d_t front,
                                  __read_only image2d_t distMap,
                                  float lowerLimit,
                                  float upperLimit)
{
    int2 coord = {get_global_id(0), get_global_id(1)};
    int2 const imageSize = (int2) (get_image_width(front), get_image_height(front));

    float2 pos =
      (float2) ((float) coord.x / (imageSize.x - 1), (float) coord.y / (imageSize.y - 1));

    float sdf = read_imagef(distMap, samplerNearestPos, pos).x;

    if ((sdf <= lowerLimit) || (sdf > upperLimit))
    {
        pos = (float2) (FLT_MAX);
    }
    write_imagef(front, coord, (float4) (pos.x, pos.y, 0.f, 0.f));
}

void kernel jumpFlood(__write_only image2d_t front, __read_only image2d_t back, int stepLength)
{
    int2 const coord = {get_global_id(0), get_global_id(1)};

    int2 const imageSize = (int2) (get_image_width(front), get_image_height(front));

    float2 pos =
      (float2) ((float) coord.x / (imageSize.x - 1), (float) coord.y / (imageSize.y - 1));

    float2 bestPos = read_imagef(back, samplerNearestCoord, coord).xy;
    float bestDist = distance(bestPos, pos);
    for (int x = -stepLength; x <= stepLength; x += stepLength)
    {
        for (int y = -stepLength; y <= stepLength; y += stepLength)
        {
            int2 neighbourCoord = (int2) (x, y) + coord;
            float2 origin = read_imagef(back, samplerNearestCoord, neighbourCoord).xy;

            float dist = distance(origin, pos);
            if (dist < bestDist && (origin.x != FLT_MAX) && (origin.y != FLT_MAX))
            {
                bestDist = dist;
                bestPos = origin;
            }
        }
    }
    write_imagef(front, coord, (float4) (bestPos, 0.f, 0.f));
}

void kernel distMapFromJfa(__read_only image2d_t jfaMap,
                           __write_only image2d_t distMap,
                           float4 buildArea)
{
    int2 const coordDistMap = {get_global_id(0), get_global_id(1)};
    int2 const imageSize = (int2) (get_image_width(distMap), get_image_height(distMap));
    float2 const normalizedPos = (float2) ((float) coordDistMap.x / (imageSize.x - 1),
                                           (float) coordDistMap.y / (imageSize.y - 1));

    float2 const origin = normalizedPosToBuildArea2f(
      read_imagef(jfaMap, samplerNearestPos, normalizedPos).xy, buildArea);
    float2 const pos = normalizedPosToBuildArea2f(normalizedPos, buildArea);
    float dist = distance(pos, origin);
    write_imagef(distMap, coordDistMap, (float4) (dist, 0.f, 0.f, 0.f));
}

void kernel renderDistMapFromJfaAndUniteNegative(__read_only image2d_t jfaMap,
                                                 __read_only image2d_t distMapPrevious,
                                                 __write_only image2d_t distMap,
                                                 float4 buildArea)
{
    int2 const coordDistMap = {get_global_id(0), get_global_id(1)};
    int2 const imageSize = (int2) (get_image_width(distMap), get_image_height(distMap));
    float2 const normalizedPos = (float2) ((float) coordDistMap.x / (imageSize.x - 1),
                                           (float) coordDistMap.y / (imageSize.y - 1));

    float const previousSdf = read_imagef(distMapPrevious, samplerNearestPos, normalizedPos).x;
    float2 const pos = normalizedPosToBuildArea2f(normalizedPos, buildArea);
    float2 const origin = normalizedPosToBuildArea2f(
      read_imagef(jfaMap, samplerNearestPos, normalizedPos).xy, buildArea);
    float dist = -distance(pos, origin);

    dist = (fabs(dist) > fabs(previousSdf)) ? dist : previousSdf;
    write_imagef(distMap, coordDistMap, (float4) (dist, 0.f, 0.f, 0.f));
}

#ifdef cl_khr_3d_image_writes
void kernel preComputeSdf(__write_only image3d_t target, struct BoundingBox bbox, PAYLOAD_ARGS)
{
    int4 const coord = {get_global_id(0), get_global_id(1), get_global_id(2), 0};
    int3 const imageSize =
      (int3) (get_image_width(target), get_image_height(target), get_image_depth(target));
    float3 normalizedPos = (float3) ((float) coord.x / (imageSize.x - 1),
                                     (float) coord.y / (imageSize.y - 1),
                                     (float) coord.z / (imageSize.z - 1));

    float3 const pos = normalizedPosToBuildVolume(normalizedPos, bbox);
    renderingSettings.approximation = AM_FULL_MODEL;
    float const sdf = modelInternal(pos, PASS_PAYLOAD_ARGS).w;
    write_imagef(target, coord, sdf);
}
#endif

void kernel calculateVertexNormals(__global float4 * vertices,
                                   __global float4 * normals,
                                   PAYLOAD_ARGS)
{
    int index = get_global_id(0);
    float3 pos = vertices[index].xyz;
    normals[index] = (float4) (surfaceNormalModelOnly(pos.xyz, PASS_PAYLOAD_ARGS).xyz, 0.f);
}





/// @brief Optimized kernel that moves points to the nearest surface with improved convergence
/// and early termination for fast bounding box computation
void kernel movePointsToSurface(__global float4 * in, __global float4 * out, PAYLOAD_ARGS)
{
    int const index = get_global_id(0);
    float3 pos = in[index].xyz;
    float3 const originalPos = pos;
    
    // Convergence parameters - tuned for bounding box computation
    float const surfaceThreshold = 0.0001f;
    float const maxTravelDistance = 20.0f;  // Allow more travel for better coverage
    int const maxIterations = 30;  // Further reduced iterations for speed
    float const minStepSize = 0.0001f;  // Minimum step to avoid infinite loops
    
    float sdf = model(pos, PASS_PAYLOAD_ARGS).w;
    float initialSdf = fabs(sdf);
    float prevSdf = sdf;
    float totalTravelDistance = 0.0f;
    int stagnationCount = 0;
    bool wasCloseToSurface = false;
    
    // Early exit if already very close to surface
    if (fabs(sdf) < surfaceThreshold)
    {
        out[index] = (float4)(pos, sdf);
        return;
    }
    
    // Early exit if very far from surface (likely not useful for bounding box)
    if (initialSdf > 50.0f)
    {
        out[index] = (float4)(originalPos, sdf);
        return;
    }
    
    for (int i = 0; i < maxIterations; ++i)
    {
        // Early termination checks
        if (fabs(sdf) <= surfaceThreshold)
        {
            break;  // Converged successfully
        }
        
        if (totalTravelDistance > maxTravelDistance)
        {
            // Traveled too far, probably diverging or no surface nearby
            break;
        }
        
        // Get surface normal with caching consideration
        float3 normal = surfaceNormalModelOnly(pos, PASS_PAYLOAD_ARGS).xyz;
        float normalLength = length(normal);
        
        // Check for degenerate normals
        if (normalLength < 0.001f)
        {
            break;  // Near singularity, exit
        }
        normal = normal / normalLength;  // Normalize
        
        // Adaptive step sizing with improved heuristics
        float stepSize;
        float absSdf = fabs(sdf);
        
        if (absSdf > 1.0f)
        {
            // Far from surface: aggressive steps but bounded
            stepSize = clamp(absSdf * 0.7f, 0.01f, 2.0f);
        }
        else if (absSdf > 0.1f)
        {
            // Medium distance: moderate steps
            stepSize = absSdf * 0.85f;
        }
        else if (absSdf > 0.01f)
        {
            // Close to surface: careful steps
            stepSize = absSdf * 0.95f;
            wasCloseToSurface = true;
        }
        else
        {
            // Very close: use direct Newton-Raphson step
            stepSize = sdf;  // Direct step to surface (signed)
            wasCloseToSurface = true;
        }
        
        // Ensure minimum step size to avoid infinite loops
        if (fabs(stepSize) < minStepSize)
        {
            stepSize = (stepSize >= 0) ? minStepSize : -minStepSize;
        }
        
        // Move towards surface
        float3 const step = normal * stepSize * sign(sdf);
        pos = pos - step;
        float stepLength = length(step);
        totalTravelDistance += stepLength;
        
        // Update SDF at new position
        prevSdf = sdf;
        sdf = model(pos, PASS_PAYLOAD_ARGS).w;
        
        // Advanced convergence checking
        float progress = fabs(sdf - prevSdf);
        float relativeProgress = (fabs(prevSdf) > 0.0001f) ? progress / fabs(prevSdf) : progress;
        
        // Check for stagnation with relative tolerance
        if (relativeProgress < 0.01f || progress < surfaceThreshold * 0.1f)
        {
            stagnationCount++;
            if (stagnationCount > 2)
            {
                break;  // Not making sufficient progress
            }
        }
        else
        {
            stagnationCount = 0;
        }
        
        // Divergence detection and correction
        if (fabs(sdf) > fabs(prevSdf) * 1.5f && i > 3)
        {
            // Diverging: try smaller step
            pos = pos + step * 0.5f;  // Backtrack halfway
            sdf = model(pos, PASS_PAYLOAD_ARGS).w;
            
            if (fabs(sdf) > fabs(prevSdf))
            {
                // Still diverging, revert to previous position and exit
                pos = pos - step * 0.5f;
                sdf = prevSdf;
                break;
            }
        }
        
        // Oscillation detection for points near thin features
        if (wasCloseToSurface && i > 10 && fabs(sdf) > absSdf * 0.8f)
        {
            // Likely oscillating around thin feature, use current best position
            break;
        }
    }
    
    // Final micro-refinement for very close points
    if (fabs(sdf) <= surfaceThreshold * 5.0f && fabs(sdf) > surfaceThreshold)
    {
        float3 const normal = surfaceNormalModelOnly(pos, PASS_PAYLOAD_ARGS).xyz;
        if (length(normal) > 0.001f)
        {
            pos = pos - normalize(normal) * sdf;  // Final Newton step
            sdf = model(pos, PASS_PAYLOAD_ARGS).w;
        }
    }

    out[index] = (float4)(pos, sdf);
}

void kernel adoptVertexOfMeshToSurface(__global float4 * in, __global float4 * out, PAYLOAD_ARGS)
{
    int const index = get_global_id(0);
    float3 pos = in[index].xyz;
    float sdf = FLT_MAX;
    float const step = 0.001f;
    float minDist = FLT_MAX;
    float3 bestPos = pos;
    for (int i = 0; i < 1000; ++i)
    {
        float3 const normal = surfaceNormalModelOnly(pos, PASS_PAYLOAD_ARGS).xyz;
        sdf = model(pos, PASS_PAYLOAD_ARGS).w;
        if (fabs(sdf) < minDist)
        {
            minDist = fabs(sdf);
            bestPos = pos;
        }
        sdf = clamp(sdf, -0.01f, 0.01f);
        sdf *= 0.01f;
        pos = pos - normal * sdf;
    }

    out[index] = (float4) (bestPos, sdf);
}

float2 posToWorldPos(float2 pos, float2 gridSize_px, float4 buildArea)
{
    float2 const size = buildArea.zw - buildArea.xy;

    float2 const cellSize =
      (float2) (size.x / (gridSize_px.x - 1.f), size.y / (gridSize_px.y - 1.f));

    return (float2) (buildArea.xy + cellSize * pos);
}

float2 coodinateToWorldPos(int2 coords, float2 gridSize_px, float4 buildArea)
{
    return posToWorldPos((float2) (coords.x, coords.y), gridSize_px, buildArea);
}

void kernel computeMarchingSquareStates(__write_only image2d_t out, float z_mm, PAYLOAD_ARGS)
{
    int2 const coords = (int2) (get_global_id(0), get_global_id(1));
    float2 const dim = (float2) (get_image_width(out), get_image_height(out));

    //FIXME Somehow the first queried value is garbage when using nanovdb, maybe there is a bug in the caching of the accessor
    float const sdf = modelForSlicing((float3) (coodinateToWorldPos(coords + (int2) (0, 0), dim, buildArea), z_mm),
                      PASS_PAYLOAD_ARGS); //just a workaround;

    bool const upRight =
      modelForSlicing((float3) (coodinateToWorldPos(coords + (int2) (0, -1), dim, buildArea), z_mm),
                      PASS_PAYLOAD_ARGS) < 0.f;

    bool const downLeft =
      modelForSlicing((float3) (coodinateToWorldPos(coords + (int2) (-1, 0), dim, buildArea), z_mm),
                      PASS_PAYLOAD_ARGS) < 0.f;

    bool const downRight =
      modelForSlicing((float3) (coodinateToWorldPos(coords + (int2) (0, 0), dim, buildArea), z_mm),
                      PASS_PAYLOAD_ARGS) < 0.f;

  bool const upLeft =
      modelForSlicing(
        (float3) (coodinateToWorldPos(coords + (int2) (-1, -1), dim, buildArea), z_mm),
        PASS_PAYLOAD_ARGS) < 0.f;
    char state = 0;
    if (upLeft)
    {
        state += 1;
    }
    if (upRight)
    {
        state += 2;
    }
    if (downLeft)
    {
        state += 4;
    }
    if (downRight)
    {
        state += 8;
    }
    
    write_imageui(out, coords, state);
}

int circularIndex(int index, int size)
{
    if (index < 0)
    {
        return size + index;
    }
    return index % size;
}

float2 normalOfLineSegment(float2 a, float2 b)
{
    return normalize((float2) ((a.y - b.y), -(a.x - b.x)));
}

void kernel adoptVertexPositions2d(__global float2 * in,
                                  __global float2 * out,
                                  int size,
                                  int numIterations,
                                  float z_mm,
                                  PAYLOAD_ARGS)
{
    uint const index = get_global_id(0);
    
    if (index == size - 1)
    {
        out[index] = in[0];
        return;
    }

    float3 pos = (float3) (in[index].xy, z_mm);
    float3 before = (float3) (in[circularIndex(index - 1, size)].xy, z_mm);
    float3 next = (float3) (in[circularIndex(index + 1, size)].xy, z_mm);

    float sdf = modelForSlicing(pos, PASS_PAYLOAD_ARGS);
    float const maxStepSize = 0.001f;
    float const minStepSize = 0.00001f;

    float2 const beforeNormal = normalOfLineSegment(before.xy, pos.xy);
    float2 const nextNormal = normalOfLineSegment(pos.xy, next.xy);
    float2 const vertexNormal = normalize(beforeNormal + nextNormal);

    float minDeviation = FLT_MAX;
    float offset = 0.f;
    float2 bestPos = pos.xy;

    float2 currentPos = pos.xy;
    for (int i = 0; i < numIterations; ++i)
    {
        offset += sign(sdf) * max(min(maxStepSize, fabs(sdf * 0.5f)), minStepSize);
        currentPos = pos.xy + offset * vertexNormal;

        float2 const beforeNormal = normalOfLineSegment(before.xy, currentPos);
        float2 const nextNormal = normalOfLineSegment(currentPos, next.xy);
        float2 const vertexNormal = normalize(beforeNormal + nextNormal);

        float2 const expectedVertexNormal =
          gradientXY5Point((float3) (currentPos, z_mm),
                           (float3) (renderingSettings.normalOffset),
                           PASS_PAYLOAD_ARGS)
            .xy;

        sdf = modelForSlicing((float3) (currentPos, z_mm), PASS_PAYLOAD_ARGS);

        float const deviation =
          (sdf * sdf) *
          (sqDistance(expectedVertexNormal, vertexNormal) +
           renderingSettings.weightMidPoint * sqDistance(beforeNormal.xy, nextNormal) +
           renderingSettings.weightDistToNb *
             (sqDistance(currentPos, before.xy) + sqDistance(currentPos, next.xy)));

        if (deviation < minDeviation)
        {
            bestPos = currentPos;
            minDeviation = deviation;
        }
    }

    out[index] = bestPos.xy;
}