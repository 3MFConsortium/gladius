#include "ResourceContext.h"
#include "ResourceManager.h"
#include "gpgpu.h"
#include "src/ImageRGBA.h"
#include "src/Profiling.h"

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>

#include <fstream>
#include <sstream>
#include <string>

namespace gladius
{
    ResourceContext::ResourceContext(SharedComputeContext context)
        : m_ComputeContext(context)
    {
        m_renderingSettings = {};
        m_renderingSettings.z_mm = 1000.0f;
        m_renderingSettings.time_s = 0.0f;
        m_renderingSettings.flags = RF_SHOW_BUILDPLATE | RF_SHOW_FIELD | RF_CUT_OFF_OBJECT | RF_SHOW_COORDINATE_SYSTEM;
        m_renderingSettings.quality = 5.0f;
        m_renderingSettings.weightDistToNb = 1000.0f;
        m_renderingSettings.weightMidPoint = 1.f;
        m_renderingSettings.normalOffset = 0.0001f;

        initResolutions();
        createBuffer();
    }

    void ResourceContext::initResolutions()
    {
        const auto clipAreaSize =
          cl_float2{{std::clamp((m_clippingArea.z - m_clippingArea.x), 1.f, 1000.f),
                     std::clamp((m_clippingArea.w - m_clippingArea.y), 1.f, 1000.f)}};

        const auto verticesPerMM = 40;
        float constexpr superSampling = 1.f;
        auto level = static_cast<float>(m_layerResolutions.size());
        for (auto & layerRes : m_layerResolutions)
        {
            layerRes = {
              {static_cast<int>(ceil(superSampling * clipAreaSize.x * verticesPerMM / level)),
               static_cast<int>(ceil(superSampling * clipAreaSize.y * verticesPerMM / level))}};
            --level;
        }
        m_sizeGrid = {{static_cast<int>(clipAreaSize.x * verticesPerMM),
                       static_cast<int>(clipAreaSize.y * verticesPerMM)}};
    }

    bool isValidClippingArea(cl_float4 clipArea)
    {
        return (clipArea.x < clipArea.z && clipArea.y < clipArea.w);
    }

    float area(cl_float4 rect)
    {
        return (rect.z - rect.x) * (rect.y - rect.w);
    }

    void ResourceContext::setClippingArea(cl_float4 clipArea, float margin)
    {

        // add margin to the clipping area
        clipArea.x -= margin;
        clipArea.y -= margin;

        clipArea.z += margin;
        clipArea.w += margin;

        if (fabs(m_clippingArea.x - clipArea.x) <= FLT_EPSILON &&
            fabs(m_clippingArea.y - clipArea.y) <= FLT_EPSILON &&
            fabs(m_clippingArea.z - clipArea.z) <= FLT_EPSILON &&
            fabs(m_clippingArea.w - clipArea.w) <= FLT_EPSILON)
        {
            return;
        }

        if ((m_clippingArea.x < clipArea.x) && (m_clippingArea.y < clipArea.y) &&
            (m_clippingArea.z > clipArea.z) && (m_clippingArea.w > clipArea.w) &&
            area(m_clippingArea) * 0.8f < area(clipArea))

        {
            // new cliparea fits into old one, and the area decrease is not worth it -> save the
            // reallocation
            return;
        }

        if (!isValidClippingArea(clipArea))
        {
            throw std::runtime_error("invalid build area");
        }

        m_clippingArea = clipArea;

        m_resizeOfBuildAreaBufferRequired = true;
        m_resizeOfDistanceMapsRequired = true;
    }

    const Vertices & ResourceContext::getVertices() const
    {
        return *m_contourVertexPos;
    }

    Vertices & ResourceContext::getVertices()
    {
        return *m_contourVertexPos;
    }

    cl_int2 ResourceContext::getGridSize() const
    {
        return m_sizeGrid;
    }

    DistanceMap & ResourceContext::getDistanceMap()
    {
        auto & lastLayer = *m_distanceMaps.back();
        return lastLayer;
    }

    void ResourceContext::swapDistanceMaps()
    {
        std::swap(m_distanceMap2d, m_distanceMaps.back());
    }

    cl_float3 ResourceContext::getEyePosition() const
    {
        return m_eyePosition;
    }

    Vertices & ResourceContext::getContourVertexPos()
    {
        return *m_contourVertexPos;
    }

    Vertices & ResourceContext::getContourVertexPosBackBuf()
    {
        return *m_contourVertexPosBackBuf;
    }

    void ResourceContext::swapContourVertexPos()
    {
        std::swap(m_contourVertexPosBackBuf, m_contourVertexPos);
    }

    DistanceMipMaps & ResourceContext::getDistanceMipMaps()
    {
        return m_distanceMaps;
    }

    DepthBuffer & ResourceContext::getDepthBuffer()
    {
        return *m_depthBuffer;
    }

    ImageRGBA & ResourceContext::getBackBuffer()
    {
        return *m_backBuffer;
    }

    Resolutions & ResourceContext::getMipMapResolutions()
    {
        return m_layerResolutions;
    }

    RenderingSettings & ResourceContext::getRenderingSettings()
    {
        return m_renderingSettings;
    }

    DepthBuffer & ResourceContext::getDistanceToBottom()
    {
        return *m_distanceToBottom;
    }

    DepthBuffer & ResourceContext::getDistanceToTop()
    {
        return *m_distanceToTop;
    }

    void ResourceContext::setEyePosition(cl_float3 eyepos)
    {
        m_eyePosition = std::move(eyepos);
    }

    void ResourceContext::setModelViewPerspectiveMat(cl_float16 mvp)
    {
        m_modelViewPerspectiveMat = std::move(mvp);
    }

    const cl_float16 & ResourceContext::getModelViewPerspectiveMat() const
    {
        return m_modelViewPerspectiveMat;
    }

    cl_float4 ResourceContext::getBuildArea() const
    {
        return m_buildArea;
    }

    cl_float4 ResourceContext::getClippingArea() const
    {
        return m_clippingArea;
    }

    float ResourceContext::getTime_s() const
    {
        const auto end = std::chrono::system_clock::now();
        const std::chrono::duration<float> diff = end - m_start;
        return diff.count();
    }

    PreComputedSdf & ResourceContext::getPrecompSdfBuffer()
    {
        return *m_preCompSdf;
    }

    ParameterBuffer & ResourceContext::getParameterBuffer()
    {
        return *m_parameter;
    }

    BoundingBoxBuffer & ResourceContext::getBoundingBox()
    {
        return *m_boundingBox;
    }

    CommandBuffer & ResourceContext::getCommandBuffer()
    {
        return *m_commands;
    }

    void ResourceContext::clearDistanceMaps()
    {
        for (const auto & distMap : m_distanceMaps)
        {
            distMap->fill({{0.0, 0.0}});
            distMap->write();
        }
    }

    void ResourceContext::requestDistanceMaps()
    {
        if (!m_resizeOfDistanceMapsRequired)
        {
            return;
        }

        initResolutions();
        m_distanceMaps.clear();
        for (const auto & res : m_layerResolutions)
        {
            m_distanceMaps.emplace_back(std::make_unique<DistanceMap>(
              *m_ComputeContext, static_cast<size_t>(res.x), static_cast<size_t>(res.y)));
        }

        for (const auto & distMap : m_distanceMaps)
        {
            distMap->allocateOnDevice();
        }
    }

    void ResourceContext::requestSliceBuffer()
    {
        if (!m_resizeOfBuildAreaBufferRequired)
        {
            return;
        }
        initResolutions();

        m_contourVertexPos =
          std::make_unique<Vertices>(*m_ComputeContext, m_sizeGrid.x, m_sizeGrid.y);
        m_contourVertexPos->allocateOnDevice();
        std::fill(m_contourVertexPos->getData().begin(),
                  m_contourVertexPos->getData().end(),
                  cl_float4{{0,
                             std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()}});
        m_contourVertexPos->write();

        m_contourVertexPosBackBuf = std::make_unique<Vertices>(*m_contourVertexPos);

        m_marchinSquareStates =
          std::make_unique<MarchingSquaresStates>(*m_ComputeContext, m_sizeGrid.x, m_sizeGrid.y);
        m_marchinSquareStates->allocateOnDevice();

        m_resizeOfBuildAreaBufferRequired = false;
    }

    void ResourceContext::initConvexHullVertices()
    {
        if (!m_convexHullVertices)
        {
            m_convexHullVertices = std::make_unique<VertexBuffer>(*m_ComputeContext);
        }

        if (!m_convexHullInitalVertices)
        {
            m_convexHullInitalVertices = std::make_unique<VertexBuffer>(*m_ComputeContext);
        }

        auto constexpr extension = 400.f;
        BoundingBox extendedBoundingBox = getBuildVolume();
        extendedBoundingBox.min.x -= extension;
        extendedBoundingBox.min.y -= extension;
        extendedBoundingBox.min.z -= extension;
        extendedBoundingBox.max.x += extension;
        extendedBoundingBox.max.y += extension;
        extendedBoundingBox.max.z += extension;

        auto & vertices = m_convexHullInitalVertices->getData();
        vertices.clear();

        auto const x_mm = extendedBoundingBox.max.x - extendedBoundingBox.min.x;
        auto const y_mm = extendedBoundingBox.max.y - extendedBoundingBox.min.y;
        auto const z_mm = extendedBoundingBox.max.z - extendedBoundingBox.min.z;

        size_t constexpr numPerAxis = 128;

        auto const x_dist = x_mm / numPerAxis;
        auto const y_dist = y_mm / numPerAxis;
        auto const z_dist = z_mm / numPerAxis;

        vertices.reserve(numPerAxis * numPerAxis * numPerAxis);

        for (size_t x = 0u; x < numPerAxis; ++x)
            for (size_t y = 0u; y < numPerAxis; ++y)
                for (size_t z = 0u; z < numPerAxis; ++z)
                {
                    cl_float4 const vertex{
                      {extendedBoundingBox.min.x + static_cast<float>(x) * x_dist,
                       extendedBoundingBox.min.y + static_cast<float>(y) * y_dist,
                       extendedBoundingBox.min.z + static_cast<float>(z) * z_dist,
                       0.f}};
                    vertices.push_back(vertex);
                }

        m_convexHullInitalVertices->write();

        m_convexHullVertices->getData().resize(m_convexHullInitalVertices->getSize());
        m_convexHullVertices->write();
    }

    VertexBuffer & ResourceContext::getConvexHullInitialVertices() const
    {
        return *m_convexHullInitalVertices;
    }

    VertexBuffer & ResourceContext::getConvexHullVertices() const
    {
        return *m_convexHullVertices;
    }

    void ResourceContext::setPreCompSdfBBox(BoundingBox bbox)
    {
        m_preCompSdfBBox = bbox;
    }

    BoundingBox const & ResourceContext::getPreCompSdfBBox() const
    {
        return m_preCompSdfBBox;
    }

    void ResourceContext::allocatePreComputedSdf(size_t sizeX, size_t sizeY, size_t sizeZ)
    {
        if (m_preCompSdf &&
            (((m_preCompSdf->getWidth() == sizeX) && (m_preCompSdf->getHeight() == sizeY)) ||
             (m_preCompSdf->getDepth() == sizeZ)))
        {
            return;
        }
        m_preCompSdf = std::make_unique<PreComputedSdf>(*m_ComputeContext, sizeX, sizeY, sizeZ);
        m_preCompSdf->allocateOnDevice();
    }

    void ResourceContext::releasePreComputedSdf()
    {
        m_preCompSdf = std::make_unique<PreComputedSdf>(
          *m_ComputeContext, 1, 1, 1); // we need at least a small dummy buffer
        m_renderingSettings.approximation = AM_FULL_MODEL;
        m_preCompSdf->allocateOnDevice();
    }

    void ResourceContext::createBuffer()
    {
        m_depthBuffer = std::make_unique<DepthBuffer>(*m_ComputeContext);
        m_depthBuffer->allocateOnDevice();

        m_backBuffer = std::make_unique<ImageRGBA>(*m_ComputeContext);
        m_backBuffer->allocateOnDevice();

        allocatePreComputedSdf();

        m_parameter = std::make_unique<ParameterBuffer>(*m_ComputeContext);
        m_parameter->create();

        m_boundingBox = std::make_unique<BoundingBoxBuffer>(*m_ComputeContext);
        m_boundingBox->getData().push_back({});
        m_boundingBox->create();

        m_commands = std::make_unique<CommandBuffer>(*m_ComputeContext);
        m_commands->create();

        initConvexHullVertices();
    }

    void ResourceContext::clearImageStacks()
    {
        m_imageStacks.clear();
    }

    void ResourceContext::addImageStack(ImageStackOCLBuffer && imageStack)
    {
        m_imageStacks.emplace_back(std::move(imageStack));
    }

    const ImageStacks & ResourceContext::getImageStacks() const
    {
        return m_imageStacks;
    }

    const BoundingBox & ResourceContext::getBuildVolume() const
    {
        return m_buildVolume;
    }

    MarchingSquaresStates & ResourceContext::getMarchingSquareStates() const
    {
        return *m_marchinSquareStates;
    }
}
