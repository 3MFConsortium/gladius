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
    // Forward declarations of shared pointer types
    using SharedGLImageBuffer = std::shared_ptr<GLImageBuffer>;
    using SharedContourExtractor = std::shared_ptr<ContourExtractor>;
    using SharedSlicerProgram = std::shared_ptr<SlicerProgram>;
    using SharedRenderProgram = std::shared_ptr<RenderProgram>;
    using SharedPrimitives = std::shared_ptr<Primitives>;

    /**
     * @brief Wrapper class for ContourExtractor to maintain backward compatibility
     * with code that expects a reference to ContourExtractor.
     */
    class ContourExtractorWrapper
    {
      public:
        explicit ContourExtractorWrapper(SharedContourExtractor contourExtractor)
            : m_contourExtractor(std::move(contourExtractor))
        {
        }

        // Forward methods to the underlying ContourExtractor
        auto & getContour()
        {
            return m_contourExtractor->getContour();
        }
        const auto & getContour() const
        {
            return m_contourExtractor->getContour();
        }
        auto & getOpenContours()
        {
            return m_contourExtractor->getOpenContours();
        }
        auto & getNormals()
        {
            return m_contourExtractor->getNormals();
        }
        const auto & getNormals() const
        {
            return m_contourExtractor->getNormals();
        }
        auto & getSourceVertices()
        {
            return m_contourExtractor->getSourceVertices();
        }
        const auto & getSourceVertices() const
        {
            return m_contourExtractor->getSourceVertices();
        }
        const auto & getSliceQuality() const
        {
            return m_contourExtractor->getSliceQuality();
        }
        void clear()
        {
            m_contourExtractor->clear();
        }
        void setSimplificationTolerance(float tol)
        {
            m_contourExtractor->setSimplificationTolerance(tol);
        }
        void addIsoLineFromMarchingSquare(MarchingSquaresStates & states,
                                          float4 const & clippingArea)
        {
            // Use non-const reference per ContourExtractor::addIsoLineFromMarchingSquare signature
            m_contourExtractor->addIsoLineFromMarchingSquare(states, clippingArea);
        }
        void runPostProcessing()
        {
            m_contourExtractor->runPostProcessing();
        }
        void calcAreas()
        {
            m_contourExtractor->calcAreas();
        }
        void calcSign()
        {
            m_contourExtractor->calcSign();
        }
        PolyLines generateOffsetContours(float offset, const PolyLines & contours) const
        {
            return m_contourExtractor->generateOffsetContours(offset, contours);
        }
        // Simple version for backward compatibility
        PolyLines generateOffsetContours(float offset) const
        {
            return m_contourExtractor->generateOffsetContours(offset,
                                                              m_contourExtractor->getContour());
        }

        // Allow direct access to the underlying shared_ptr when needed
        SharedContourExtractor getSharedPtr() const
        {
            return m_contourExtractor;
        }

        // Allow using the wrapper as a ContourExtractor reference
        operator ContourExtractor &()
        {
            return *m_contourExtractor;
        }
        operator const ContourExtractor &() const
        {
            return *m_contourExtractor;
        }

      private:
        SharedContourExtractor m_contourExtractor;
    };

    /**
     * @brief Wrapper class for ResourceContext to maintain backward compatibility
     * with code that expects a reference to ResourceContext.
     */
    class ResourceContextWrapper
    {
      public:
        explicit ResourceContextWrapper(SharedResources resources)
            : m_resources(std::move(resources))
        {
        } // Forward methods to the underlying ResourceContext
        auto & getRenderingSettings()
        {
            return m_resources->getRenderingSettings();
        }
        auto & getParameterBuffer()
        {
            return m_resources->getParameterBuffer();
        }
        auto & getCommandBuffer()
        {
            return m_resources->getCommandBuffer();
        }
        auto & getPrecompSdfBuffer()
        {
            return m_resources->getPrecompSdfBuffer();
        }
        auto getClippingArea() const
        {
            return m_resources->getClippingArea();
        }
        void setClippingArea(cl_float4 area, float padding = 0.0f)
        {
            m_resources->setClippingArea(area, padding);
        }
        MarchingSquaresStates & getMarchingSquareStates()
        {
            return m_resources->getMarchingSquareStates();
        }
        void requestSliceBuffer()
        {
            m_resources->requestSliceBuffer();
        }
        void requestDistanceMaps()
        {
            m_resources->requestDistanceMaps();
        }
        DistanceMipMaps & getDistanceMipMaps()
        {
            return m_resources->getDistanceMipMaps();
        }
        auto getEyePosition() const
        {
            return m_resources->getEyePosition();
        }
        void setEyePosition(cl_float4 position)
        {
            m_resources->setEyePosition(position);
        }
        auto getModelViewPerspectiveMat() const
        {
            return m_resources->getModelViewPerspectiveMat();
        }
        void setModelViewPerspectiveMat(cl_float16 mat)
        {
            m_resources->setModelViewPerspectiveMat(mat);
        }
        auto & getConvexHullVertices()
        {
            return m_resources->getConvexHullVertices();
        }
        auto & getConvexHullInitialVertices()
        {
            return m_resources->getConvexHullInitialVertices();
        }
        void initConvexHullVertices()
        {
            m_resources->initConvexHullVertices();
        }
        void allocatePreComputedSdf(size_t width = 0, size_t height = 0, size_t depth = 0)
        {
            m_resources->allocatePreComputedSdf(width, height, depth);
        }
        void setPreCompSdfBBox(const BoundingBox & box)
        {
            m_resources->setPreCompSdfBBox(box);
        }
        void releasePreComputedSdf()
        {
            m_resources->releasePreComputedSdf();
        }
        void clearImageStacks()
        {
            m_resources->clearImageStacks();
        }

        // Allow direct access to the underlying shared_ptr when needed
        SharedResources getSharedPtr() const
        {
            return m_resources;
        }

        // Allow using the wrapper as a ResourceContext reference
        operator ResourceContext &()
        {
            return *m_resources;
        }
        operator const ResourceContext &() const
        {
            return *m_resources;
        }

      private:
        SharedResources m_resources;

        friend class ComputeCore; // Allow ComputeCore to access private members
    };

    /**
     * @brief Wrapper class for ComputeContext to maintain backward compatibility
     * with code that expects a reference to ComputeContext.
     */
    class ComputeContextWrapper
    {
      public:
        explicit ComputeContextWrapper(SharedComputeContext context)
            : m_context(std::move(context))
        {
        } // Forward methods to the underlying ComputeContext
        bool isValid() const
        {
            return m_context->isValid();
        }
        const cl::CommandQueue & GetQueue()
        {
            return m_context->GetQueue();
        }
        OutputMethod outputMethod() const
        {
            return m_context->outputMethod();
        }

        // Allow direct access to the underlying shared_ptr when needed
        SharedComputeContext getSharedPtr() const
        {
            return m_context;
        }

        // Allow using the wrapper as a ComputeContext reference
        operator ComputeContext &()
        {
            return *m_context;
        }
        operator const ComputeContext &() const
        {
            return *m_context;
        }

      private:
        SharedComputeContext m_context;

        friend class ComputeCore; // Allow ComputeCore to access private members
    };

    /**
     * @brief Wrapper class for ModelState to maintain backward compatibility
     * with code that expects a reference to ModelState.
     */
    class ModelStateWrapper
    {
      public:
        explicit ModelStateWrapper(std::shared_ptr<ModelState> modelState)
            : m_modelState(std::move(modelState))
        {
        }

        // Forward methods to the underlying ModelState
        bool isModelUpToDate() const
        {
            return m_modelState->isModelUpToDate();
        }
        void signalCompilationStarted()
        {
            m_modelState->signalCompilationStarted();
        }
        void signalCompilationFinished()
        {
            m_modelState->signalCompilationFinished();
        }

        // Allow direct access to the underlying shared_ptr when needed
        std::shared_ptr<ModelState> getSharedPtr() const
        {
            return m_modelState;
        }

        // Allow using the wrapper as a ModelState reference
        operator ModelState &()
        {
            return *m_modelState;
        }
        operator const ModelState &() const
        {
            return *m_modelState;
        }

      private:
        std::shared_ptr<ModelState> m_modelState;

        friend class ComputeCore; // Allow ComputeCore to access private members
    }; /**
        * @brief Wrapper class for Primitives to maintain backward compatibility
        * with code that expects a reference to Primitives.
        */
    class PrimitivesWrapper
    {
      public:
        explicit PrimitivesWrapper(SharedPrimitives primitives)
            : m_primitives(std::move(primitives))
        {
        }

        // Forward methods to access the underlying Primitives data
        auto & data()
        {
            return m_primitives->data;
        }
        const auto & data() const
        {
            return m_primitives->data;
        }

        // Allow direct access to the underlying shared_ptr when needed
        SharedPrimitives getSharedPtr() const
        {
            return m_primitives;
        }

        // Allow using the wrapper as a Primitives reference
        operator Primitives &()
        {
            return *m_primitives;
        }
        operator const Primitives &() const
        {
            return *m_primitives;
        }

      private:
        SharedPrimitives m_primitives;

        friend class ComputeCore; // Allow ComputeCore to access private members
    };

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

        /// @brief Prepares the compute core for thumbnail generation in headless mode
        /// @return true if preparation succeeded, false otherwise
        bool prepareThumbnailGeneration();
        [[nodiscard]] SharedGLImageBuffer getResultImage() const;
        [[nodiscard]] SharedContourExtractor getContour() const;

        [[nodiscard]] cl_float getSliceHeight() const;

        void setSliceHeight(cl_float z_mm);

        [[nodiscard]] SharedSlicerProgram getSlicerProgram() const;
        [[nodiscard]] SharedRenderProgram getBestRenderProgram() const;
        [[nodiscard]] SharedRenderProgram getPreviewRenderProgram() const;
        [[nodiscard]] SharedRenderProgram getOptimzedRenderProgram() const;

        bool setScreenResolution(size_t width, size_t height);
        bool setLowResPreviewResolution(size_t width, size_t height);
        [[nodiscard]] std::pair<size_t, size_t> getLowResPreviewResolution() const;
        [[nodiscard]] SharedPrimitives getPrimitives() const;

        [[nodiscard]] SharedResources getResourceContext() const;

        void generateSdfSlice() const;
        [[nodiscard]] std::optional<BoundingBox> getBoundingBox() const;
        void updateClippingAreaWithPadding() const;
        void updateClippingAreaToBoundingBox() const;
        [[nodiscard]] bool isVdbRequired() const;

        [[nodiscard]] bool isAnyCompilationInProgress() const;

        bool updateBBox();
        void updateBBoxOrThrow();

        void refreshProgram(nodes::SharedAssembly assembly);
        void tryRefreshProgramProtected(nodes::SharedAssembly assembly);

        [[nodiscard]] bool isRendererReady() const;

        [[nodiscard]] SharedComputeContext getComputeContext() const;

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

        [[nodiscard]] std::shared_ptr<ModelState> getMeshResourceState() const;

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
        SharedContourExtractor m_contour;
        SharedGLImageBuffer m_resultImage;
        SharedGLImageBuffer m_lowResPreviewImage;
        std::shared_ptr<ImageRGBA> m_thumbnailImage;
        std::shared_ptr<ImageRGBA> m_thumbnailImageHighRes;

        SharedPrimitives m_primitives;
        SharedComputeContext m_ComputeContext;
        SharedResources m_resources;

        double layerThickness_mm = 0.05;
        cl_float m_sliceHeight_mm{0.0f};

        cl_float m_lastContourSliceHeight_mm{0.0f};

        std::optional<BoundingBox> m_boundingBox{};
        bool m_isComputationTimeLoggingEnabled = false;

        RequiredCapabilities m_capabilities = RequiredCapabilities::OpenGLInterop;
        events::SharedLogger m_eventLogger;

        std::shared_ptr<ModelState> m_meshResourceState;

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
