#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include <fmt/core.h>
#include <lodepng.h>

#include "CliReader.h"
#include "ComputeCore.h"
#include "Contour.h"
#include "Mesh.h"
#include "Profiling.h"
#include "RenderProgram.h"
#include "ResourceContext.h"
#include "SlicerProgram.h"
#include "ToCommandStreamVisitor.h"
#include "ToOCLVisitor.h"
#include "compute/ComputeCore.h"
#include "gpgpu.h"
#include "nodes/OptimizeOutputs.h"
#include "nodes/Validator.h"

namespace gladius
{
    ComputeCore::ComputeCore(SharedComputeContext context,
                             RequiredCapabilities requiredCapabilities,
                             events::SharedLogger logger)
        : m_contour(std::make_shared<ContourExtractor>(logger))
        , m_ComputeContext(context)
        , m_resources(std::make_shared<ResourceContext>(context))
        , m_capabilities(requiredCapabilities)
        , m_eventLogger(logger)
        , m_programs(context, requiredCapabilities, logger, m_resources)
        , m_meshResourceState(std::make_shared<ModelState>())
    {
        init();
    }

    void ComputeCore::init()
    {
        LOG_LOCATION
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        createBuffer();
        m_programs.init();
    }

    void ComputeCore::reset()
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        m_boundingBox.reset();
        m_programs.reset();
        setSliceHeight(0.f);
    }

    ComputeToken ComputeCore::waitForComputeToken()
    {
        return ComputeToken(m_computeMutex);
    }

    OptionalComputeToken ComputeCore::requestComputeToken()
    {
        if (!m_computeMutex.try_lock())
        {
            return {};
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);
        return OptionalComputeToken(m_computeMutex);
    }

    void ComputeCore::createBuffer()
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        const auto width = size_t{256};
        const auto height = size_t{256};

        LOG_LOCATION

        m_primitives = std::make_shared<Primitives>(*m_ComputeContext);
        m_primitives->create();

        if (m_capabilities == RequiredCapabilities::OpenGLInterop)
        {
            LOG_LOCATION
            m_resultImage = std::make_shared<GLImageBuffer>(*m_ComputeContext, width, height);
            m_resultImage->allocateOnDevice();

            m_lowResPreviewImage =
              std::make_shared<GLImageBuffer>(*m_ComputeContext, width / 2, height / 2);
            m_lowResPreviewImage->allocateOnDevice();
        }

        const auto thumbnailSize = size_t{256};
        m_thumbnailImage =
          std::make_shared<ImageRGBA>(*m_ComputeContext, thumbnailSize, thumbnailSize);
        m_thumbnailImage->allocateOnDevice();

        m_thumbnailImageHighRes =
          std::make_shared<ImageRGBA>(*m_ComputeContext, thumbnailSize * 2, thumbnailSize * 2);
        m_thumbnailImageHighRes->allocateOnDevice();

        m_resources->allocatePreComputedSdf();
    }
    void ComputeCore::generateContourInternal(nodes::SliceParameter const & sliceParameter)
    {
        ProfileFunction

          m_resources->getRenderingSettings()
            .approximation = AM_FULL_MODEL;
        m_contour->clear();

        generateContourMarchingSquare(sliceParameter);
    }

    void ComputeCore::generateContourMarchingSquare(nodes::SliceParameter const & sliceParameter)
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        m_resources->requestSliceBuffer();
        m_primitives->write();
        m_programs.getSlicerProgram()->computeMarchingSquareState(*m_primitives,
                                                                  sliceParameter.zHeight_mm);
        m_contour->addIsoLineFromMarchingSquare(m_resources->getMarchingSquareStates(),
                                                m_resources->getClippingArea());

        if (sliceParameter.adoptGradientBased)
        {
            for (auto & contour : m_contour->getContour())
            {
                if (contour.vertices.empty())
                {
                    continue;
                }
                Vertex2dBuffer verticesIn(*m_ComputeContext);

                for (auto & vertex : contour.vertices)
                {
                    verticesIn.getData().push_back({{vertex.x(), vertex.y()}});
                }

                auto verticesOut = verticesIn;

                m_programs.getSlicerProgram()->adoptVertexPositions2d(
                  *m_primitives, verticesIn, verticesOut, sliceParameter.zHeight_mm);
                int i = 0;
                for (auto & vertex : contour.vertices)
                {
                    cl_float2 const adoptedVertex = verticesOut.getData().at(i);
                    vertex.x() = adoptedVertex.x;
                    vertex.y() = adoptedVertex.y;
                    ++i;
                }
            }
        }
        m_contour->runPostProcessing();
        m_lastContourSliceHeight_mm = sliceParameter.zHeight_mm;
    }

    bool ComputeCore::tryToupdateParameter(nodes::Assembly & assembly)
    {
        ProfileFunction

          if (!m_computeMutex.try_lock())
        {
            LOG_LOCATION
            return false;
        }

        return updateParameterBlocking(assembly);
    }

    bool ComputeCore::updateParameterBlocking(nodes::Assembly & assembly)
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex, std::adopt_lock);
        if (isAutoUpdateBoundingBoxEnabled())
        {
            resetBoundingBox();
        }

        auto & paramBuf = getResourceContext()->getParameterBuffer();
        auto & parameter = paramBuf.getData();
        parameter.clear();

        int currentIndex = 0;
        for (auto & model : assembly.getFunctions())
        {
            if (!model.second)
            {
                continue;
            }
            for (auto [id, param] : model.second->getParameterRegistry())
            {
                if (param == nullptr || param->getId() != id)
                {
                    continue;
                }

                auto const varParam = dynamic_cast<nodes::VariantParameter *>(param);

                if (varParam == nullptr)
                {
                    return false;
                }

                if (varParam && !varParam->getSource().has_value())
                {
                    auto & variant = varParam->Value();
                    if (auto const typedValuePtr = std::get_if<float>(&variant))
                    {
                        param->setLookUpIndex(currentIndex);
                        parameter.push_back(*typedValuePtr);
                        ++currentIndex;
                    }
                    if (auto const typedValuePtr = std::get_if<int>(&variant))
                    {
                        param->setLookUpIndex(currentIndex);
                        parameter.push_back(static_cast<float>(*typedValuePtr));
                        ++currentIndex;
                    }
                    if (auto const typedValuePtr = std::get_if<nodes::float3>(&variant))
                    {
                        param->setLookUpIndex(currentIndex);
                        parameter.push_back(typedValuePtr->x);
                        parameter.push_back(typedValuePtr->y);
                        parameter.push_back(typedValuePtr->z);
                        currentIndex += 3;
                    }

                    if (nodes::Matrix4x4 * const mat = std::get_if<nodes::Matrix4x4>(&variant))
                    {
                        nodes::Matrix4x4 const & matrix = *mat;
                        param->setLookUpIndex(currentIndex);

                        for (int row = 0; row < 4; ++row)
                        {
                            for (int col = 0; col < 4; ++col)
                            {
                                float const value = matrix[row][col];
                                parameter.push_back(value);
                                ++currentIndex;
                            }
                        }
                    }
                }
            }
        }

        paramBuf.write();
        invalidatePreCompSdf();
        LOG_LOCATION
        return true;
    }

    void ComputeCore::setPreCompSdfSize(size_t size)
    {
        m_preCompSdfSize = size;
    }

    void ComputeCore::adoptVertexOfMeshToSurface(VertexBuffer & vertices)
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        m_primitives->write();

        auto inputVertices = vertices;
        m_programs.getSlicerProgram()->adoptVertexOfMeshToSurface(
          *m_primitives, inputVertices, vertices);
    }

    void ComputeCore::setAutoUpdateBoundingBox(bool autoUpdateBoundingBox)
    {
        m_autoUpdateBoundingBox = autoUpdateBoundingBox;
    }

    bool ComputeCore::isAutoUpdateBoundingBoxEnabled() const
    {
        return m_autoUpdateBoundingBox;
    }

    void ComputeCore::generateContours(nodes::SliceParameter sliceParameter)
    {
        ProfileFunction;

        if (!updateBBox())
        {
            logMsg("Bounding box computation failed");
            return;
        }

        updateClippingAreaWithPadding();
        generateContourInternal(sliceParameter);
    }

    void ComputeCore::generateSdfSlice() const
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);

        m_primitives->write();
        m_programs.getSlicerProgram()->renderLayers(*m_primitives, 0.0f, m_sliceHeight_mm);
    }

    std::optional<BoundingBox> ComputeCore::getBoundingBox() const
    {
        return m_boundingBox; // Return a copy instead of a reference
    }

    void ComputeCore::updateClippingAreaWithPadding() const
    {
        ProfileFunction auto constexpr padding = 10.f;
        cl_float4 const newClippingArea{{m_boundingBox->min.x - padding,
                                         m_boundingBox->min.y - padding,
                                         m_boundingBox->max.x + padding,
                                         m_boundingBox->max.y + padding}};

        if (isValidClippingArea(newClippingArea))
        {
            m_resources->setClippingArea(newClippingArea, padding);
        }
    }

    void ComputeCore::updateClippingAreaToBoundingBox() const
    {
        ProfileFunction

          if (!m_boundingBox)
        {
            throw std::runtime_error("Bounding box is not available");
        }

        cl_float4 const newClippingArea{
          {m_boundingBox->min.x, m_boundingBox->min.y, m_boundingBox->max.x, m_boundingBox->max.y}};

        if (isValidClippingArea(newClippingArea))
        {
            m_resources->setClippingArea(newClippingArea);
        }
    }

    bool ComputeCore::isBusy() const
    {
        return !(m_precompSdfIsValid && isAnyCompilationInProgress() && isRendererReady());
    }

    bool ComputeCore::updateBoundingBoxFast()
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);

        if (m_boundingBox && !std::isinf(m_boundingBox->min.x) &&
            !std::isinf(m_boundingBox->max.x) && !std::isinf(m_boundingBox->min.y) &&
            !std::isinf(m_boundingBox->max.y) && !std::isinf(m_boundingBox->min.z) &&
            !std::isinf(m_boundingBox->max.z))
        {
            return true;
        }

        if (!m_programs.getSlicerState().isModelUpToDate())
        {
            try
            {
                logMsg("updateBoundingBoxFast: slicer state not up to date, requesting recompile");
                recompileIfRequired();
                logMsg("updateBoundingBoxFast: after recompileIfRequired: " +
                       m_programs.getDebugStateSummary());
            }
            catch (...)
            {
            }
            LOG_LOCATION
            return false;
        }

        m_resources->initConvexHullVertices();
        auto const & vertices = m_resources->getConvexHullVertices().getData();

        m_boundingBox = BoundingBox{};

        if (!m_programs.getSlicerProgram()->isValid())
        {
            try
            {
                logMsg("updateBoundingBoxFast: slicer program invalid");
            }
            catch (...)
            {
            }
            LOG_LOCATION
            return false;
        }

        try
        {
            m_programs.getSlicerProgram()->movePointsToSurface(
              *m_primitives,
              m_resources->getConvexHullInitialVertices(),
              m_resources->getConvexHullVertices());
        }
        catch (std::exception const & e)
        {
            logMsg(std::string("updateBoundingBoxFast: movePointsToSurface exception: ") +
                   e.what());
            return false;
        }

        CL_ERROR(m_ComputeContext->GetQueue().finish());
        m_resources->getConvexHullVertices().read();
        for (auto const & vertex : vertices)
        {
            if (fabs(vertex.w) > 0.01f)
            {
                continue;
            }

            m_boundingBox->min.x = (!isnan(vertex.x) && !isinf(vertex.x))
                                     ? std::min(m_boundingBox->min.x, vertex.x)
                                     : m_boundingBox->min.x;
            m_boundingBox->min.y = (!isnan(vertex.y) && !isinf(vertex.y))
                                     ? std::min(m_boundingBox->min.y, vertex.y)
                                     : m_boundingBox->min.y;
            m_boundingBox->min.z = (!isnan(vertex.z) && !isinf(vertex.z))
                                     ? std::min(m_boundingBox->min.z, vertex.z)
                                     : m_boundingBox->min.z;

            m_boundingBox->max.x = (!isnan(vertex.x) && !isinf(vertex.x))
                                     ? std::max(m_boundingBox->max.x, vertex.x)
                                     : m_boundingBox->max.x;
            m_boundingBox->max.y = (!isnan(vertex.y) && !isinf(vertex.y))
                                     ? std::max(m_boundingBox->max.y, vertex.y)
                                     : m_boundingBox->max.y;
            m_boundingBox->max.z = (!isnan(vertex.z) && !isinf(vertex.z))
                                     ? std::max(m_boundingBox->max.z, vertex.z)
                                     : m_boundingBox->max.z;
        }

        // if the bounding box values are not finite, use the build volume as bounding box
        if (!std::isfinite(m_boundingBox->min.x) || !std::isfinite(m_boundingBox->max.x) ||
            !std::isfinite(m_boundingBox->min.y) || !std::isfinite(m_boundingBox->max.y) ||
            !std::isfinite(m_boundingBox->min.z) || !std::isfinite(m_boundingBox->max.z))
        {
            m_boundingBox = BoundingBox{{0.f, 0.f, 0.f}, {400.f, 400.f, 400.f}};
        }
        LOG_LOCATION
        return true;
    }

    void ComputeCore::recompileIfRequired()
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        m_programs.recompileIfRequired();
        LOG_LOCATION
    }

    void ComputeCore::recompileBlockingNoLock()
    {
        m_programs.recompileBlockingNoLock();
    }

    void ComputeCore::resetBoundingBox()
    {
        m_boundingBox.reset();
    }

    BitmapLayer ComputeCore::generateDownSkinMap(float z_mm, Vector2 pixelSize_mm)
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        setSliceHeight(z_mm);
        float2 const pixelSize{{pixelSize_mm.x(), pixelSize_mm.y()}};

        updateBoundingBoxFast();
        updateClippingAreaWithPadding();

        auto const area = m_resources->getClippingArea();

        BitmapLayer downSkinMap;
        downSkinMap.position = Vector2{area.x, area.y};

        DepthBuffer downSkinBuffer{*m_ComputeContext};
        auto const size = determineBufferSize(pixelSize);
        downSkinBuffer.setWidth(size.x);
        downSkinBuffer.setHeight(size.y);
        downSkinBuffer.allocateOnDevice();
        m_programs.getSlicerProgram()->renderDownSkinDistance(
          downSkinBuffer, *m_primitives, m_sliceHeight_mm);

        downSkinMap.pixelSize = pixelSize_mm;
        downSkinMap.bitmapData = std::move(downSkinBuffer.getData());
        downSkinMap.width_px = downSkinBuffer.getWidth();
        downSkinMap.height_px = downSkinBuffer.getHeight();
        return downSkinMap;
    }

    BitmapLayer ComputeCore::generateUpSkinMap(float z_mm, Vector2 pixelSize_mm)
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        setSliceHeight(z_mm);
        float2 const pixelSize{{pixelSize_mm.x(), pixelSize_mm.y()}};

        updateBoundingBoxFast();
        updateClippingAreaWithPadding();

        auto const area = m_resources->getClippingArea();

        BitmapLayer upSkinMap;
        upSkinMap.position = Vector2{area.x, area.y};

        DepthBuffer upSkinBuffer{*m_ComputeContext};
        auto const size = determineBufferSize(pixelSize);
        upSkinBuffer.setWidth(size.x);
        upSkinBuffer.setHeight(size.y);
        upSkinBuffer.allocateOnDevice();
        m_programs.getSlicerProgram()->renderUpSkinDistance(
          upSkinBuffer, *m_primitives, m_sliceHeight_mm);

        upSkinMap.pixelSize = pixelSize_mm;
        upSkinMap.bitmapData = std::move(upSkinBuffer.getData());
        upSkinMap.width_px = upSkinBuffer.getWidth();
        upSkinMap.height_px = upSkinBuffer.getHeight();
        return upSkinMap;
    }

    SharedComputeContext ComputeCore::getComputeContext() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        return m_ComputeContext;
    }

    void ComputeCore::setComputeContext(std::shared_ptr<ComputeContext> context)
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        m_ComputeContext = std::move(context);
        reset();
        init();
    }

    bool ComputeCore::requestContourUpdate(nodes::SliceParameter sliceParameter)
    {
        ProfileFunction

          if (!m_computeMutex.try_lock())
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);

        if (fabs(m_lastContourSliceHeight_mm - sliceParameter.zHeight_mm) < FLT_EPSILON)
        {
            return false;
        }

        if (m_sliceFuture.valid())
        {
            m_sliceFuture.get();
        }
        m_sliceFuture = std::async(
          [&, sliceParameter]()
          {
              FrameMarkEnd("Slicing");
              std::lock_guard<std::mutex> lockContourExtractor(m_contourExtractorMutex);
              generateContours(sliceParameter);
              FrameMarkEnd("Slicing");
          });
        return true;
    }

    bool ComputeCore::isSlicingInProgress() const
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        if (!m_sliceFuture.valid())
        {
            return false;
        }

        if (fabs(m_lastContourSliceHeight_mm - m_sliceHeight_mm) < FLT_EPSILON)
        {
            return false;
        }
        return m_sliceFuture.wait_for(std::chrono::seconds(0)) != std::future_status::ready;
    }

    std::mutex & ComputeCore::getContourExtractorMutex()
    {
        return m_contourExtractorMutex;
    }

    void ComputeCore::throwIfNoOpenGL() const
    {
        if (m_capabilities == RequiredCapabilities::ComputeOnly)
        {
            throw std::runtime_error("Operation requires OpenGL which is not available");
        }
    }

    bool ComputeCore::isVdbRequired() const
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        if (!m_primitives)
        {
            return false;
        }

        const auto metaDataIter =
          std::find_if(std::begin(m_primitives->primitives.getData()),
                       std::end(m_primitives->primitives.getData()),
                       [](auto & metadata)
                       {
                           return (metadata.primitiveType == SDF_VDB) ||
                                  (metadata.primitiveType == SDF_VDB_FACE_INDICES);
                       });

        return metaDataIter != std::end(m_primitives->primitives.getData());
    }

    [[nodiscard]] bool ComputeCore::isAnyCompilationInProgress() const
    {
        return m_programs.isAnyCompilationInProgress();
    }

    bool ComputeCore::updateBBox()
    {
        ProfileFunction

          return updateBoundingBoxFast();
    }

    void ComputeCore::updateBBoxOrThrow()
    {
        ProfileFunction if (!updateBBox())
        {
            throw std::runtime_error("Bounding box computation failed");
        }
    }

    void ComputeCore::refreshProgram(nodes::SharedAssembly assembly)
    {
        ProfileFunction;

        if (!assembly)
        {
            return;
        }

        if (!assembly->assemblyModel())
        {
            return;
        }

        if (assembly->assemblyModel()->getSize() == 0u)
        {
            return;
        }

        m_boundingBox.reset();
        invalidatePreCompSdf();
        if (m_codeGenerator == CodeGenerator::CommandStream)
        {
            std::stringstream modelKernel;
            getResourceContext()->getCommandBuffer().clear();

            nodes::ToCommandStreamVisitor toCommandStreamVisitor(
              &getResourceContext()->getCommandBuffer(), assembly.get());
            try
            {
                assembly->visitAssemblyNodes(toCommandStreamVisitor);
                toCommandStreamVisitor.write(modelKernel);
            }
            catch (const std::exception & e)
            {
                logMsg(e.what());
                return;
            }

            getResourceContext()->getCommandBuffer().write();

            m_programs.setModelSource(modelKernel.str());
        }

        if (m_codeGenerator == CodeGenerator::Code)
        {
            std::stringstream optimizedKernel;
            nodes::ToOclVisitor visitor;
            assembly->visitNodes(visitor);
            visitor.write(optimizedKernel);

            m_programs.setModelSource(optimizedKernel.str());
        }
    }

    void ComputeCore::tryRefreshProgramProtected(nodes::SharedAssembly assembly)
    {
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        refreshProgram(assembly);
    }
    [[nodiscard]] bool ComputeCore::isRendererReady() const
    {
        if (!m_meshResourceState->isModelUpToDate())
        {
            return false;
        }
        return (!getBestRenderProgram()->isCompilationInProgress());
    }

    void ComputeCore::compileSlicerProgramBlocking()
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        m_programs.recompileBlockingNoLock();

        updateBBox();
    }

    void ComputeCore::logMsg(std::string msg) const
    {
        if (!m_eventLogger)
        {
            std::cerr << msg << "\n";
            return;
        }
        getLogger().addEvent({std::move(msg), events::Severity::Info});
    }

    void ComputeCore::computeVertexNormals(Mesh & mesh) const
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        mesh.write();
        m_programs.getSlicerProgram()->calculateNormals(*m_primitives, mesh);
        mesh.read();
    }

    events::Logger & ComputeCore::getLogger() const
    {
        if (!m_eventLogger)
        {
            throw std::runtime_error("logger is missing");
        }
        return *m_eventLogger;
    }

    cl_int2 ComputeCore::determineBufferSize(float2 pixelSize_mm) const
    {
        auto const rect = m_resources->getClippingArea();

        auto const w = rect.z - rect.x;
        auto const h = rect.w - rect.y;
        return {{static_cast<int>(ceil(w / pixelSize_mm.x)), //
                 static_cast<int>(ceil(h / pixelSize_mm.y))}};
    }

    void ComputeCore::reinitIfNecssary()
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        if (m_ComputeContext->isValid())
        {
            return;
        }
        m_eventLogger->addEvent({"Reinitializing compute context"});

        reset();
        init();
    }

    [[nodiscard]] int ComputeCore::layerNumber() const
    {
        if (layerThickness_mm < std::numeric_limits<double>::epsilon())
        {
            throw std::runtime_error("Layer thickness cannot be zero or negative");
        }
        return static_cast<int>(
          std::round(static_cast<double>(m_sliceHeight_mm) / layerThickness_mm));
    }

    bool ComputeCore::precomputeSdfForWholeBuildPlatform()
    {
        ProfileFunction

          if (!m_programs.getSlicerState().isModelUpToDate())
        {
            recompileIfRequired();
            return false;
        }

        if (!m_programs.getSlicerProgram()->isValid())
        {
            return false;
        }

        if (m_precompSdfIsValid)
        {
            return true;
        }
        updateBBox();

        if (!m_boundingBox.has_value())
        {
            return false;
        }

        std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        auto const margin = 10.f;
        auto prevCompSdfBBox = m_boundingBox.value_or(
          BoundingBox{float4{0.f, 0.f, 0.f, 0.f}, float4{400.f, 400.f, 400.f, 0.f}});
        prevCompSdfBBox.min.x -= margin;
        prevCompSdfBBox.min.y -= margin;
        prevCompSdfBBox.min.z -= margin;

        prevCompSdfBBox.max.x += margin;
        prevCompSdfBBox.max.y += margin;
        prevCompSdfBBox.max.z += margin;

        m_resources->allocatePreComputedSdf(m_preCompSdfSize, m_preCompSdfSize, m_preCompSdfSize);
        m_resources->setPreCompSdfBBox(prevCompSdfBBox);
        m_programs.getSlicerProgram()->precomputeSdf(*m_primitives, prevCompSdfBBox);
        m_precompSdfIsValid = true;
        return true;
    }

    void ComputeCore::precomputeSdfForBBox(const BoundingBox & boundingBox)
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);

        m_resources->allocatePreComputedSdf(m_preCompSdfSize, m_preCompSdfSize, m_preCompSdfSize);
        m_resources->setPreCompSdfBBox(boundingBox);
        m_programs.getSlicerProgram()->precomputeSdf(*m_primitives, boundingBox);
    }

    bool ComputeCore::prepareImageRendering()
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);

        try
        {
            // Caveman logs for headless diagnostics
            try
            {
                std::stringstream ss;
                ss << "ComputeCore.prepareThumbnailGeneration: begin"
                   << " glInterop=" << (m_capabilities == RequiredCapabilities::OpenGLInterop)
                   << " precompValid=" << (m_precompSdfIsValid ? 1 : 0) << " renderProgValid="
                   << (m_programs.getRenderProgram() &&
                       !m_programs.getRenderProgram()->isCompilationInProgress())
                   << " slicerValid="
                   << (m_programs.getSlicerProgram() && m_programs.getSlicerProgram()->isValid());
                logMsg(ss.str());
            }
            catch (...)
            {
            }
            // Ensure model is compiled and up to date
            if (!m_programs.getSlicerState().isModelUpToDate())
            {
                // Add explicit debug about model source and states
                try
                {
                    logMsg(std::string(
                             "prepareThumbnailGeneration: slicer not up to date; hasModelSource=") +
                           (m_programs.hasModelSource() ? "1" : "0"));
                    logMsg("prepareThumbnailGeneration: before recompile: " +
                           m_programs.getDebugStateSummary());
                }
                catch (...)
                {
                }

                recompileIfRequired();

                // Check again after recompilation
                if (!m_programs.getSlicerState().isModelUpToDate())
                {
                    // Try a blocking compile as a last resort in headless mode
                    try
                    {
                        logMsg("prepareThumbnailGeneration: retry with blocking compile");
                    }
                    catch (...)
                    {
                    }
                    m_programs.recompileBlockingNoLock();
                    if (!m_programs.getSlicerState().isModelUpToDate())
                    {
                        logMsg("Model compilation failed during thumbnail preparation (blocking)");
                        return false;
                    }
                }
                try
                {
                    logMsg("prepareThumbnailGeneration: after compile: " +
                           m_programs.getDebugStateSummary());
                }
                catch (...)
                {
                }
            }

            // Ensure SDF is precomputed
            if (!precomputeSdfForWholeBuildPlatform())
            {
                logMsg("SDF precomputation failed during thumbnail preparation");
                return false;
            }

            // Ensure bounding box is valid
            updateBBox();
            if (!m_boundingBox.has_value())
            {
                logMsg("Bounding box computation failed during thumbnail preparation");
                return false;
            }

            auto const & bb = m_boundingBox.value();
            if (std::isnan(bb.min.x) || std::isnan(bb.min.y) || std::isnan(bb.min.z) ||
                std::isnan(bb.max.x) || std::isnan(bb.max.y) || std::isnan(bb.max.z))
            {
                logMsg("Bounding box contains invalid values during thumbnail preparation");
                return false;
            }

            try
            {
                std::stringstream ss2;
                ss2 << "ComputeCore.prepareThumbnailGeneration: OK bbox min(" << bb.min.x << ","
                    << bb.min.y << "," << bb.min.z << ") max(" << bb.max.x << "," << bb.max.y << ","
                    << bb.max.z << ")";
                logMsg(ss2.str());
            }
            catch (...)
            {
            }

            logMsg("Thumbnail generation preparation completed successfully");
            return true;
        }
        catch (std::exception const & e)
        {
            logMsg("Exception during thumbnail preparation: " + std::string(e.what()));
            return false;
        }
    }

    SharedGLImageBuffer ComputeCore::getResultImage() const
    {
        return m_resultImage;
    }

    SharedContourExtractor ComputeCore::getContour() const
    {
        // Create a local copy of the future to avoid race conditions
        // We must NOT hold the mutex while waiting to avoid deadlock
        std::future<void> localFuture;
        {
            std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
            if (m_sliceFuture.valid())
            {
                // Move the future to avoid race conditions
                localFuture = std::move(const_cast<std::future<void> &>(m_sliceFuture));
            }
        }

        // Wait for the future outside the mutex to avoid deadlock
        if (localFuture.valid())
        {
            localFuture.get();
        }

        // Now acquire the mutex to safely return the contour
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        return m_contour;
    }

    cl_float ComputeCore::getSliceHeight() const
    {
        return m_sliceHeight_mm;
    }

    void ComputeCore::setSliceHeight(cl_float z_mm)
    {
        m_resources->getRenderingSettings().z_mm = z_mm;

        m_sliceHeight_mm = z_mm;
    }
    SharedSlicerProgram ComputeCore::getSlicerProgram() const
    {
        return std::shared_ptr<SlicerProgram>(m_programs.getSlicerProgram(),
                                              [](SlicerProgram *) {}); // Non-owning shared_ptr
    }
    SharedRenderProgram ComputeCore::getBestRenderProgram() const
    {
        return std::shared_ptr<RenderProgram>(m_programs.getRenderProgram(),
                                              [](RenderProgram *) {}); // Non-owning shared_ptr
    }
    SharedRenderProgram ComputeCore::getPreviewRenderProgram() const
    {
        return std::shared_ptr<RenderProgram>(m_programs.getRenderProgram(),
                                              [](RenderProgram *) {}); // Non-owning shared_ptr
    }
    SharedRenderProgram ComputeCore::getOptimzedRenderProgram() const
    {
        return std::shared_ptr<RenderProgram>(m_programs.getRenderProgram(),
                                              [](RenderProgram *) {}); // Non-owning shared_ptr
    }

    bool ComputeCore::setScreenResolution(size_t width, size_t height)
    {
        ProfileFunction if (m_resultImage && (width == m_resultImage->getWidth()) &&
                            (height == m_resultImage->getHeight()))
        {
            return false;
        }
        if (!m_computeMutex.try_lock())
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);

        m_resultImage = std::make_shared<GLImageBuffer>(*m_ComputeContext, width, height);
        m_resultImage->allocateOnDevice();
        return true;
    }

    bool ComputeCore::setLowResPreviewResolution(size_t width, size_t height)
    {
        if (m_lowResPreviewImage && (width == m_lowResPreviewImage->getWidth()) &&
            (height == m_lowResPreviewImage->getHeight()))
        {
            return false;
        }
        if (!m_computeMutex.try_lock())
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);

        m_lowResPreviewImage = std::make_shared<GLImageBuffer>(*m_ComputeContext, width, height);
        m_lowResPreviewImage->allocateOnDevice();
        return true;
    }

    std::pair<size_t, size_t> ComputeCore::getLowResPreviewResolution() const
    {
        if (!m_lowResPreviewImage)
        {
            return {0u, 0u};
        }
        return {m_lowResPreviewImage->getWidth(), m_lowResPreviewImage->getHeight()};
    }

    SharedPrimitives ComputeCore::getPrimitives() const
    {
        return m_primitives;
    }

    SharedResources ComputeCore::getResourceContext() const
    {
        return m_resources;
    }

    void ComputeCore::renderResultImageInterOp(DistanceMap & sourceImage,
                                               GLImageBuffer & targetImage) const
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        throwIfNoOpenGL();
        cl_int err = 0;
        CL_ERROR(m_ComputeContext->GetQueue().finish());
        CL_ERROR(err);
        std::vector<cl::Memory> memObjects;
        memObjects.clear();
        memObjects.push_back(targetImage.getBuffer());
        cl::Event events;

        err = m_ComputeContext->GetQueue().enqueueAcquireGLObjects(&memObjects, nullptr, &events);
        CL_ERROR(err);
        CL_ERROR(events.wait());

        renderResultImageReadPixel(sourceImage, targetImage);

        CL_ERROR(err);
        err = m_ComputeContext->GetQueue().enqueueReleaseGLObjects(&memObjects);
        CL_ERROR(err);
        CL_ERROR(events.wait());

        CL_ERROR(m_ComputeContext->GetQueue().finish());
        CL_ERROR(err);
    }

    void ComputeCore::renderResultImageReadPixel(DistanceMap & sourceImage,
                                                 GLImageBuffer & targetImage) const
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        throwIfNoOpenGL();
        m_programs.getSlicerProgram()->renderResultImageReadPixel(sourceImage, targetImage);
    }

    void ComputeCore::renderImage(DistanceMap & sourceImage) const
    {
        ProfileFunction LOG_LOCATION

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        throwIfNoOpenGL();
        glFinish();

        if (m_resultImage->getWidth() != sourceImage.getWidth() ||
            m_resultImage->getHeight() != sourceImage.getHeight())
        {
            m_resultImage->setWidth(sourceImage.getWidth());
            m_resultImage->setHeight(sourceImage.getHeight());

            m_resultImage->allocateOnDevice();
        }

        if (m_ComputeContext->outputMethod() == OutputMethod::interop)
        {
            this->renderResultImageInterOp(sourceImage, *m_resultImage);
        }
        if (m_ComputeContext->outputMethod() == OutputMethod::readpixel)
        {
            this->renderResultImageReadPixel(sourceImage, *m_resultImage);
        }
        LOG_LOCATION
    }

    bool ComputeCore::renderScene(size_t startLine, size_t endLine)
    {
        ProfileFunction

          if (!m_computeMutex.try_lock())
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);
        throwIfNoOpenGL();
        recompileIfRequired();

        if (getBestRenderProgram()->isCompilationInProgress())
        {
            LOG_LOCATION
            return false;
        }

        glFinish();

        m_resources->getRenderingSettings().approximation = AM_HYBRID;
        getBestRenderProgram()->renderScene(
          *m_primitives, *m_resultImage, m_sliceHeight_mm, startLine, endLine);
        m_resources->getRenderingSettings().approximation = AM_FULL_MODEL;

        m_resultImage->invalidateContent();

        LOG_LOCATION
        return true;
    }

    void ComputeCore::renderLowResPreview() const
    {
        ProfileFunction

          if (!m_computeMutex.try_lock())
        {
            return;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);
        throwIfNoOpenGL();

        glFinish();

        if (!m_precompSdfIsValid)
        {
            LOG_LOCATION
            return;
        }

        m_resources->getRenderingSettings().approximation = AM_ONLY_PRECOMPSDF;

        getBestRenderProgram()->renderScene(*m_primitives,
                                            *m_lowResPreviewImage,
                                            m_sliceHeight_mm,
                                            0,
                                            m_lowResPreviewImage->getHeight());
        m_resources->getRenderingSettings().approximation = AM_FULL_MODEL;
        getBestRenderProgram()->resample(
          *m_lowResPreviewImage, *m_resultImage, 0, m_resultImage->getHeight());
        m_resultImage->invalidateContent();
        LOG_LOCATION
    }

    void ComputeCore::invalidatePreCompSdf()
    {
        m_precompSdfIsValid = false;
    }

    events::SharedLogger ComputeCore::getSharedLogger() const
    {
        return m_eventLogger;
    }

    CodeGenerator ComputeCore::getCodeGenerator() const
    {
        return m_codeGenerator;
    }

    void ComputeCore::setCodeGenerator(CodeGenerator generator)
    {
        m_codeGenerator = generator;
    }
    std::shared_ptr<ModelState> ComputeCore::getMeshResourceState() const
    {
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        return m_meshResourceState;
    }

    PlainImage ComputeCore::createThumbnail()
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        if (!m_thumbnailImage || !m_thumbnailImageHighRes)
        {
            logMsg("ComputeCore.createThumbnail: thumbnail images not initialized");
            throw std::runtime_error("Thumbnail image is not initialized");
        }

        if (m_codeGenerator != CodeGenerator::CommandStream &&
            !m_programs.getRendererState().isModelUpToDate())
        {
            logMsg("ComputeCore.createThumbnail: renderer state not up to date");
            throw std::runtime_error("Model is not up to date");
        }

        if (!m_precompSdfIsValid)
        {
            logMsg("ComputeCore.createThumbnail: precomputed SDF is not valid");
            throw std::runtime_error("Precomputed SDF is not valid");
        }

        // Only call glFinish if OpenGL is available
        if (m_capabilities == RequiredCapabilities::OpenGLInterop)
        {
            glFinish();
        }
        updateBBox();
        if (!m_boundingBox.has_value())
        {
            logMsg("ComputeCore.createThumbnail: no bounding box available");
            throw std::runtime_error("Bounding box is not valid");
        }

        auto bb = getBoundingBox().value();
        if (std::isnan(bb.min.x) || std::isnan(bb.min.y) || std::isnan(bb.min.z) ||
            std::isnan(bb.max.x) || std::isnan(bb.max.y) || std::isnan(bb.max.z))
        {
            logMsg("ComputeCore.createThumbnail: bounding box invalid values");
            throw std::runtime_error("Bounding box is not valid");
        }

        auto backupEyePosition = m_resources->getEyePosition();
        auto backupViewPerspectiveMat = m_resources->getModelViewPerspectiveMat();

        ui::OrbitalCamera thumbnailCamera;
        const auto thumbnailSize = 256.0f;

        thumbnailCamera.setAngle(0.6f, -2.0f);
        thumbnailCamera.centerView(bb);
        thumbnailCamera.update(10000.f);
        thumbnailCamera.adjustDistanceToTarget(bb, thumbnailSize, thumbnailSize);

        thumbnailCamera.update(10000.f);

        applyCamera(thumbnailCamera);

        m_resources->getRenderingSettings().approximation = AM_FULL_MODEL;
        getBestRenderProgram()->renderScene(
          *m_primitives, *m_thumbnailImageHighRes, 0, 0, m_thumbnailImageHighRes->getHeight());

        m_resources->setEyePosition(backupEyePosition);
        m_resources->setModelViewPerspectiveMat(backupViewPerspectiveMat);

        getBestRenderProgram()->resample(
          *m_thumbnailImageHighRes, *m_thumbnailImage, 0, m_thumbnailImage->getHeight());

        m_thumbnailImage->read();

        auto data = m_thumbnailImage->getData();

        unsigned int width = static_cast<unsigned int>(m_thumbnailImage->getWidth());
        unsigned int height = static_cast<unsigned int>(m_thumbnailImage->getHeight());

        PlainImage image;
        image.width = width;
        image.height = height;

        for (unsigned int i = 0; i < width * height; ++i)
        {
            image.data.push_back(
              static_cast<unsigned char>(std::clamp(data[i].x * 255, 0.0f, 255.0f)));
            image.data.push_back(
              static_cast<unsigned char>(std::clamp(data[i].y * 255, 0.0f, 255.0f)));
            image.data.push_back(
              static_cast<unsigned char>(std::clamp(data[i].z * 255, 0.0f, 255.0f)));
            image.data.push_back(static_cast<unsigned char>(255));
        }

        return image;
    }

    PlainImage ComputeCore::createThumbnailPng()
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);

        auto image = createThumbnail();

        PlainImage pngImage;
        pngImage.width = image.width;
        pngImage.height = image.height;

        lodepng::encode(pngImage.data,
                        image.data,
                        static_cast<unsigned int>(image.width),
                        static_cast<unsigned int>(image.height));
        return pngImage;
    }

    void ComputeCore::saveThumbnail(std::filesystem::path const & filename)
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        auto image = createThumbnail();

        // Save the image as a PNG file
        lodepng::encode(filename.string(),
                        image.data,
                        static_cast<unsigned int>(image.width),
                        static_cast<unsigned int>(image.height));
    }
    void ComputeCore::applyCamera(ui::OrbitalCamera const & camera)
    {
        getResourceContext()->setEyePosition(camera.getEyePosition());
        getResourceContext()->setModelViewPerspectiveMat(
          camera.computeModelViewPerspectiveMatrix());
    }

    void ComputeCore::injectSmoothingKernel(std::string const & kernel)
    {
        if (!m_kernelReplacements)
        {
            m_kernelReplacements = std::make_shared<KernelReplacements>();
        }

        m_kernelReplacements->insert_or_assign("// <SMOOTHING KERNEL>", kernel);
    }
}
