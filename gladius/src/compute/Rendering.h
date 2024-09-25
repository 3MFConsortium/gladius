#pragma once

#include <BitmapLayer.h>
#include <ContourExtractor.h>
#include <EventLogger.h>
#include <GLImageBuffer.h>
#include <ImageRGBA.h>
#include <ModelState.h>
#include <RenderProgram.h>
#include <ResourceContext.h>
#include <SlicerProgram.h>
#include <nodes/BuildParameter.h>
#include <nodes/Model.h>
#include <nodes/nodesfwd.h>
#include <ui/OrbitalCamera.h>

#include <compute/types.h>

namespace gladius
{
    class Rendering
    {
      public:
        explicit Rendering(SharedComputeContext context,  RequiredCapabilities requiredCapabilities, events::SharedLogger logger);

        void init();
        void reset();

        void createBuffer();

        [[nodiscard]] bool renderScene(size_t startLine, size_t endLine);
        void renderLowResPreview() const;


        [[nodiscard]] GLImageBuffer * getResultImage() const;

        ContourExtractor & getContour();

        [[nodiscard]] cl_float getSliceHeight() const;

        void setSliceHeight(cl_float z_mm);

        [[nodiscard]] RenderProgram * getPreviewRenderProgram() const;
        [[nodiscard]] RenderProgram * getOptimzedRenderProgram() const;

        bool setScreenResolution(size_t width, size_t height);
        bool setLowResPreviewResolution(size_t width, size_t height);

        ResourceContext & getResourceContext() const;

        [[nodiscard]] const std::optional<BoundingBox> & getBoundingBox() const;
        void updateClippingArea() const;

        [[nodiscard]] bool isRendererReady() const;

        [[nodiscard]] ComputeContext & getComputeContext() const;


        void logMsg(std::string msg) const;


        void setComputeContext(std::shared_ptr<ComputeContext> context);


        PlainImage createThumbnail();
        PlainImage createThumbnailPng();
        void saveThumbnail(std::filesystem::path const & filename);

        void applyCamera(ui::OrbitalCamera const & camera);

        bool isBusy() const;


      private:

        void throwIfNoOpenGL() const;
        [[nodiscard]] events::Logger & getLogger() const;

        cl_int2 determineBufferSize(float2 pixelSize_mm) const;
        void reinitIfNecssary();

        [[nodiscard]] int layerNumber() const;

        void renderResultImageInterOp(DistanceMap & sourceImage, GLImageBuffer & targetImage) const;
        void renderResultImageReadPixel(DistanceMap & sourceImage,
                                        GLImageBuffer & targetImage) const;
        void renderImage(DistanceMap & sourceImage) const;
        mutable std::recursive_mutex m_computeMutex; // TODO: replace with std::mutex
        
        events::SharedLogger m_eventLogger;
           RequiredCapabilities m_capabilities = RequiredCapabilities::OpenGLInterop;
        std::unique_ptr<GLImageBuffer> m_resultImage;
        std::unique_ptr<GLImageBuffer> m_lowResPreviewImage;
        std::unique_ptr<ImageRGBA> m_thumbnailImage;
        std::unique_ptr<ImageRGBA> m_thumbnailImageHighRes;

        std::unique_ptr<Primitives> m_primitives;
        SharedComputeContext m_ComputeContext;
        SharedResources m_resources;
        std::unique_ptr<SlicerProgram> m_slicerProgram;

        std::unique_ptr<RenderProgram> m_optimizedRenderProgram;
        std::unique_ptr<RenderProgram> m_previewRenderProgram;

        double layerThickness_mm = 0.05;
        cl_float m_sliceHeight_mm{0.0f};

        cl_float m_lastContourSliceHeight_mm{0.0f};

        std::optional<BoundingBox> m_boundingBox{};
    };
}
