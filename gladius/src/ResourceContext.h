#pragma once

#include "Buffer.h"
#include "ImageRGBA.h"
#include "ImageStackOCLBuffer.h"
#include "Primitives.h"
#include "ResourceManager.h"

#include <CL/cl_platform.h>

#include <chrono>
#include <vector>

namespace gladius
{
    using DistanceMipMaps = std::vector<std::unique_ptr<DistanceMap>>;
    using Resolutions = std::array<cl_int2, 4>;
    using ParameterBuffer = Buffer<cl_float>;
    using BoundingBoxBuffer = Buffer<BoundingBox>;
    using NanoVdbGridBuffer = Buffer<cl_float>;
    using CommandBuffer = Buffer<Command>;
    using VertexBuffer = Buffer<cl_float4>;
    using Vertex2dBuffer = Buffer<cl_float2>;

    using Images = std::vector<ImageObject>;

    using ImageStacks = std::vector<ImageStackOCLBuffer>;

    bool isValidClippingArea(cl_float4 clipArea);

    class ResourceContext
    {
      public:
        explicit ResourceContext(SharedComputeContext context);

        void initResolutions();

        void setClippingArea(cl_float4 clipArea, float margin = 0.0f);

        [[nodiscard]] const Vertices & getVertices() const;
        Vertices & getVertices();

        [[nodiscard]] cl_int2 getGridSize() const;

        DistanceMap & getDistanceMap();
        void swapDistanceMaps();

        [[nodiscard]] cl_float3 getEyePosition() const;

        Vertices & getContourVertexPos();
        Vertices & getContourVertexPosBackBuf();
        void swapContourVertexPos();

        DistanceMipMaps & getDistanceMipMaps();

        DepthBuffer & getDepthBuffer();
        ImageRGBA & getBackBuffer();

        Resolutions & getMipMapResolutions();
        RenderingSettings & getRenderingSettings();

        DepthBuffer & getDistanceToBottom();
        DepthBuffer & getDistanceToTop();

        void setEyePosition(cl_float3 eyepos);

        void setModelViewPerspectiveMat(cl_float16 mvp);
        [[nodiscard]] const cl_float16 & getModelViewPerspectiveMat() const;

        [[nodiscard]] cl_float4 getBuildArea() const;
        [[nodiscard]] cl_float4 getClippingArea() const;
        [[nodiscard]] float getTime_s() const;

        PreComputedSdf & getPrecompSdfBuffer();

        ParameterBuffer & getParameterBuffer();

        BoundingBoxBuffer & getBoundingBox();

        CommandBuffer & getCommandBuffer();

        void createBuffer();

        void clearImageStacks();
        void addImageStack(ImageStackOCLBuffer && imageStack);
        [[nodiscard]] const ImageStacks & getImageStacks() const;

        [[nodiscard]] const BoundingBox & getBuildVolume() const;

        void allocatePreComputedSdf(size_t sizeX = 128, size_t sizeY = 128, size_t sizeZ = 128);
        void releasePreComputedSdf();
        void requestDistanceMaps();
        void requestSliceBuffer();

        void initConvexHullVertices();

        VertexBuffer & getConvexHullInitialVertices() const;
        VertexBuffer & getConvexHullVertices() const;

        void setPreCompSdfBBox(BoundingBox bbox);
        BoundingBox const & getPreCompSdfBBox() const;

        MarchingSquaresStates & getMarchingSquareStates() const;

      private:
        void clearDistanceMaps();
        std::unique_ptr<Vertices> m_contourVertexPos;
        std::unique_ptr<Vertices> m_contourVertexPosBackBuf;

        std::unique_ptr<MarchingSquaresStates> m_marchinSquareStates;

        SharedComputeContext m_ComputeContext;

        DistanceMipMaps m_distanceMaps;
        std::unique_ptr<DistanceMap> m_distanceMap2d;

        std::unique_ptr<DepthBuffer> m_depthBuffer;
        std::unique_ptr<ImageRGBA> m_backBuffer;

        std::unique_ptr<DepthBuffer> m_distanceToTop;
        std::unique_ptr<DepthBuffer> m_distanceToBottom;

        cl_int2 m_sizeGrid{128, 128};
        Resolutions m_layerResolutions{cl_int2{256, 256},
                                       cl_int2{1024, 1024},
                                       cl_int2{4096, 4096},
                                       cl_int2{8000, 8000}};

        cl_float4 m_clippingArea{0.0f, 0.0f, 1.0f, 1.0f};
        const cl_float4 m_buildArea{0.0f, 0.0f, 400.0f, 400.0f};
        const BoundingBox m_buildVolume{{m_buildArea.x, m_buildArea.y, 0.f, 0.f},
                                        {m_buildArea.z, m_buildArea.w, 400.0f}};

        BoundingBox m_preCompSdfBBox{};

        cl_float16 m_modelViewPerspectiveMat{};
        cl_float3 m_eyePosition{500.0f, 500.0f, 500.0f};

        std::chrono::time_point<std::chrono::system_clock> m_start =
          std::chrono::system_clock::now();
        RenderingSettings m_renderingSettings{};

        std::unique_ptr<PreComputedSdf> m_preCompSdf;

        std::unique_ptr<ParameterBuffer> m_parameter;

        std::unique_ptr<BoundingBoxBuffer> m_boundingBox;

        std::unique_ptr<VertexBuffer> m_convexHullVertices;
        std::unique_ptr<VertexBuffer> m_convexHullInitalVertices;

        std::unique_ptr<CommandBuffer> m_commands;

        ImageStacks m_imageStacks;

        bool m_resizeOfBuildAreaBufferRequired = true;
        bool m_resizeOfDistanceMapsRequired = true;
    };

    using SharedResources = std::shared_ptr<ResourceContext>;
}
