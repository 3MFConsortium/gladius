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
#include <compute/Rendering.h>
#include <compute/types.h>
#include <nodes/BuildParameter.h>
#include <nodes/Model.h>
#include <nodes/nodesfwd.h>
#include <ui/OrbitalCamera.h>

#include <compute/ProgramManager.h>

#include <mutex>

namespace gladius
{

    class ComputeCore
    {
      public:
        explicit ComputeCore(SharedComputeContext context,
                             RequiredCapabilities requiredCapabilities,
                             events::SharedLogger logger);

        void init();
        void reset();

        ComputeToken waitForComputeToken();
        OptionalComputeToken requestComputeToken();

        void createBuffer();

        [[nodiscard]] bool renderScene(size_t startLine, size_t endLine);
        void renderLowResPreview() const;

        bool precomputeSdfForWholeBuildPlatform();
        void precomputeSdfForBBox(const BoundingBox & boundingBox);

        [[nodiscard]] GLImageBuffer * getResultImage() const;

        ContourExtractor & getContour();

        [[nodiscard]] cl_float getSliceHeight() const;

        void setSliceHeight(cl_float z_mm);

        [[nodiscard]] SlicerProgram * getSlicerProgram() const;
        [[nodiscard]] RenderProgram * getBestRenderProgram() const;
        [[nodiscard]] RenderProgram * getPreviewRenderProgram() const;
        [[nodiscard]] RenderProgram * getOptimzedRenderProgram() const;

        bool setScreenResolution(size_t width, size_t height);
        bool setLowResPreviewResolution(size_t width, size_t height);

        Primitives & getPrimitves() const;

        ResourceContext & getResourceContext() const;

        void generateSdfSlice() const;
        [[nodiscard]] const std::optional<BoundingBox> & getBoundingBox() const;
        void updateClippingAreaWithPadding() const;
        void updateClippingAreaToBoundingBox() const;
        [[nodiscard]] bool isVdbRequired() const;

        [[nodiscard]] bool isAnyCompilationInProgress() const;

        bool updateBBox();
        void updateBBoxOrThrow();

        void refreshProgram(nodes::SharedAssembly assembly);
        void tryRefreshProgramProtected(nodes::SharedAssembly assembly);

        [[nodiscard]] bool isRendererReady() const;

        [[nodiscard]] ComputeContext & getComputeContext() const;

        void compileSlicerProgramBlocking();

        void logMsg(std::string msg) const;

        void computeVertexNormals(Mesh & mesh) const;

        void recompileIfRequired();
        void recompileBlockingNoLock();

        void resetBoundingBox();

        BitmapLayer generateDownSkinMap(float z_mm, Vector2 pixelSize_mm);
        BitmapLayer generateUpSkinMap(float z_mm, Vector2 pixelSize_mm);

        void setComputeContext(std::shared_ptr<ComputeContext> context);

        bool requestContourUpdate(nodes::SliceParameter sliceParameter);

        bool isSlicingInProgress() const;

        std::mutex & getContourExtractorMutex();

        void invalidatePreCompSdf();

        [[nodiscard]] events::SharedLogger getSharedLogger() const;

        [[nodiscard]] CodeGenerator getCodeGenerator() const;

        void setCodeGenerator(CodeGenerator generator);

        ModelState & getMeshResourceState();

        PlainImage createThumbnail();
        PlainImage createThumbnailPng();
        void saveThumbnail(std::filesystem::path const & filename);

        void applyCamera(ui::OrbitalCamera const & camera);

        void injectSmoothingKernel(std::string const & kernel);

        bool isBusy() const;

        [[nodiscard]] bool tryToupdateParameter(nodes::Assembly & assembly);
        [[nodiscard]] bool updateParameterBlocking(nodes::Assembly & assembly);

        void setPreCompSdfSize(size_t size);

        void adoptVertexOfMeshToSurface(VertexBuffer & vertices);

        void setAutoUpdateBoundingBox(bool autoUpdateBoundingBox);
        [[nodiscard]] bool isAutoUpdateBoundingBoxEnabled() const;

      private:
        bool updateBoundingBoxFast();
        void throwIfNoOpenGL() const;
        [[nodiscard]] events::Logger & getLogger() const;

        cl_int2 determineBufferSize(float2 pixelSize_mm) const;
        void reinitIfNecssary();

        [[nodiscard]] int layerNumber() const;

        void renderResultImageInterOp(DistanceMap & sourceImage, GLImageBuffer & targetImage) const;
        void renderResultImageReadPixel(DistanceMap & sourceImage,
                                        GLImageBuffer & targetImage) const;
        void renderImage(DistanceMap & sourceImage) const;
        void generateContours(nodes::SliceParameter sliceParameter);
        void generateContourInternal(nodes::SliceParameter const & sliceParameter);
        void generateContourMarchingSquare(nodes::SliceParameter const & sliceParameter);

        mutable std::recursive_mutex m_computeMutex; // TODO: replace with std::mutex

        ContourExtractor m_contour;
        std::unique_ptr<GLImageBuffer> m_resultImage;
        std::unique_ptr<GLImageBuffer> m_lowResPreviewImage;
        std::unique_ptr<ImageRGBA> m_thumbnailImage;
        std::unique_ptr<ImageRGBA> m_thumbnailImageHighRes;

        std::unique_ptr<Primitives> m_primitives;
        SharedComputeContext m_ComputeContext;
        SharedResources m_resources;

        double layerThickness_mm = 0.05;
        cl_float m_sliceHeight_mm{0.0f};

        cl_float m_lastContourSliceHeight_mm{0.0f};

        std::optional<BoundingBox> m_boundingBox{};
        bool m_isComputationTimeLoggingEnabled = false;

        RequiredCapabilities m_capabilities = RequiredCapabilities::OpenGLInterop;
        events::SharedLogger m_eventLogger;

        ModelState m_meshResourceState;

        std::future<void> m_sliceFuture;
        std::mutex m_contourExtractorMutex;

        bool m_precompSdfIsValid = false;
        size_t m_preCompSdfSize = 256u;

        bool m_autoUpdateBoundingBox = true;

        CodeGenerator m_codeGenerator = CodeGenerator::Code;

        SharedKernelReplacements m_kernelReplacements;

        ProgramManager m_programs;
    };
}
