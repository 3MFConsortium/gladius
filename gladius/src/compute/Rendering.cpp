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
#include "compute/Rendering.h"
#include "Mesh.h"
#include "ToCommandStreamVisitor.h"
#include "ToOCLVisitor.h"
#include "gpgpu.h"
#include "Contour.h"
#include "Profiling.h"
#include "RenderProgram.h"
#include "ResourceContext.h"
#include "SlicerProgram.h"
#include "nodes/GraphFlattener.h"
#include "nodes/OptimizeOutputs.h"

namespace gladius
{
    Rendering::Rendering(SharedComputeContext context,
                            RequiredCapabilities requiredCapabilities,
                             events::SharedLogger logger)
        : 
         m_ComputeContext(context)
        , m_capabilities(requiredCapabilities)
        , m_resources(std::make_shared<ResourceContext>(context))
        , m_eventLogger(logger)

    {
        init();
    }

    void Rendering::init()
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);
        m_resources = std::make_shared<ResourceContext>(m_ComputeContext);
        createBuffer();
   
        m_optimizedRenderProgram = std::make_unique<RenderProgram>(m_ComputeContext, m_resources);
        m_previewRenderProgram = std::make_unique<RenderProgram>(m_ComputeContext, m_resources);
        m_previewRenderProgram->setEnableVdb(false);

        m_optimizedRenderProgram->buildKernelLib();


    }

    void Rendering::reset()
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        m_boundingBox.reset();


        setSliceHeight(0.f);
    }

    void Rendering::createBuffer()
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        const auto width = size_t{256};
        const auto height = size_t{256};

        m_primitives = std::make_unique<Primitives>(*m_ComputeContext);
        m_primitives->create();

        if (m_capabilities == RequiredCapabilities::OpenGLInterop)
        {
            m_resultImage = std::make_unique<GLImageBuffer>(*m_ComputeContext, width, height);
            m_resultImage->allocateOnDevice();
        }

        const auto thumbnailSize = size_t{256};
        m_thumbnailImage =
          std::make_unique<ImageRGBA>(*m_ComputeContext, thumbnailSize, thumbnailSize);
        m_thumbnailImage->allocateOnDevice();

        m_thumbnailImageHighRes =
          std::make_unique<ImageRGBA>(*m_ComputeContext, thumbnailSize * 2, thumbnailSize * 2);
        m_thumbnailImageHighRes->allocateOnDevice();

        m_resources->allocatePreComputedSdf();
    }

   
    const std::optional<BoundingBox> & Rendering::getBoundingBox() const
    {
        return m_boundingBox;
    }

    void Rendering::updateClippingArea() const
    {
        ProfileFunction auto constexpr padding = 10.f;
        cl_float4 const newClippingArea{{m_boundingBox->min.x - padding,
                                         m_boundingBox->min.y - padding,
                                         m_boundingBox->max.x + padding,
                                         m_boundingBox->max.y + padding}};

        if (isValidClippingArea(newClippingArea))
        {
            m_resources->setClippingArea(newClippingArea);
        }
    }

    void Rendering::setComputeContext(std::shared_ptr<ComputeContext> context)
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        m_ComputeContext = std::move(context);
        reset();
        init();
    }


    void Rendering::throwIfNoOpenGL() const
    {
        if (m_capabilities == RequiredCapabilities::ComputeOnly)
        {
            throw std::runtime_error("Operation requires OpenGL which is not available");
        }
    }

    ComputeContext & Rendering::getComputeContext() const
    {
        return *m_ComputeContext;
    }


    void Rendering::logMsg(std::string msg) const
    {
        if (!m_eventLogger)
        {
            std::cout << msg << "\n";
            return;
        }
        getLogger().addEvent({std::move(msg), events::Severity::Info});
    }

    events::Logger & Rendering::getLogger() const
    {
        if (!m_eventLogger)
        {
            throw std::runtime_error("logger is missing");
        }
        return *m_eventLogger;
    }

    cl_int2 Rendering::determineBufferSize(float2 pixelSize_mm) const
    {
        auto const rect = m_resources->getClippingArea();

        auto const w = rect.z - rect.x;
        auto const h = rect.w - rect.y;
        return {{static_cast<int>(ceil(w / pixelSize_mm.x)), //
                 static_cast<int>(ceil(h / pixelSize_mm.y))}};
    }

    void Rendering::reinitIfNecssary()
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

    [[nodiscard]] int Rendering::layerNumber() const
    {
        if (layerThickness_mm < std::numeric_limits<double>::epsilon())
        {
            throw std::runtime_error("Layer thickness cannot be zero or negative");
        }
        return static_cast<int>(
          std::round(static_cast<double>(m_sliceHeight_mm) / layerThickness_mm));
    }

    GLImageBuffer * Rendering::getResultImage() const
    {
        return m_resultImage.get();
    }


    cl_float Rendering::getSliceHeight() const
    {
        return m_sliceHeight_mm;
    }

    void Rendering::setSliceHeight(cl_float z_mm)
    {
        m_resources->getRenderingSettings().z_mm = z_mm;

        m_sliceHeight_mm = z_mm;
    }

    bool Rendering::setScreenResolution(size_t width, size_t height)
    {
        if (!m_computeMutex.try_lock())
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);

        if (m_resultImage && (width == m_resultImage->getWidth()) &&
            (height == m_resultImage->getHeight()))
        {
            return false;
        }
        m_resultImage = std::make_unique<GLImageBuffer>(*m_ComputeContext, width, height);
        m_resultImage->allocateOnDevice();
        return true;
    }

    bool Rendering::setLowResPreviewResolution(size_t width, size_t height)
    {
        if (!m_computeMutex.try_lock())
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);

        if (m_lowResPreviewImage && (width == m_lowResPreviewImage->getWidth()) &&
            (height == m_lowResPreviewImage->getHeight()))
        {
            return false;
        }
        m_lowResPreviewImage = std::make_unique<GLImageBuffer>(*m_ComputeContext, width, height);
        m_lowResPreviewImage->allocateOnDevice();
        return true;
    }

    ResourceContext & Rendering::getResourceContext() const
    {
        return *m_resources;
    }

    void Rendering::renderResultImageInterOp(DistanceMap & sourceImage,
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

    void Rendering::renderResultImageReadPixel(DistanceMap & sourceImage,
                                                 GLImageBuffer & targetImage) const
    {
        ProfileFunction

          std::lock_guard<std::recursive_mutex>
            lock(m_computeMutex);
        throwIfNoOpenGL();
        m_slicerProgram->renderResultImageReadPixel(sourceImage, targetImage);
    }

    void Rendering::renderImage(DistanceMap & sourceImage) const
    {
        ProfileFunction

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
    }

    bool Rendering::renderScene(size_t startLine, size_t endLine)
    {
        ProfileFunction

          if (!m_computeMutex.try_lock())
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);
        throwIfNoOpenGL();
        // recompileIfRequired(); 

        // if (getBestRenderProgram()->isCompilationInProgress())
        // {
        //     return false;
        // }

        glFinish();

        m_resources->getRenderingSettings().approximation = AM_HYBRID;
        // getBestRenderProgram()->renderScene(
        //   *m_primitives, *m_resultImage, m_sliceHeight_mm, startLine, endLine);
        m_resources->getRenderingSettings().approximation = AM_FULL_MODEL;

        m_resultImage->invalidateContent();
        return true;
    }

    void Rendering::renderLowResPreview() const
    {
        ProfileFunction

          if (!m_computeMutex.try_lock())
        {
            return;
        }
        std::lock_guard<std::recursive_mutex> lock(m_computeMutex, std::adopt_lock);
        throwIfNoOpenGL();

        glFinish();


        m_resources->getRenderingSettings().approximation = AM_ONLY_PRECOMPSDF;

        // getBestRenderProgram()->renderScene(*m_primitives,
        //                                     *m_lowResPreviewImage,
        //                                     m_sliceHeight_mm,
        //                                     0,
        //                                     m_lowResPreviewImage->getHeight());
        m_resources->getRenderingSettings().approximation = AM_FULL_MODEL;
        // getBestRenderProgram()->resample(
        //   *m_lowResPreviewImage, *m_resultImage, 0, m_resultImage->getHeight());
        m_resultImage->invalidateContent();
    }


    PlainImage Rendering::createThumbnail()
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        if (!m_thumbnailImage || !m_thumbnailImageHighRes)
        {
            throw std::runtime_error("Thumbnail image is not initialized");
        }

        // if (m_codeGenerator != CodeGenerator::CommandStream &&
        //     !m_optimizedPreviewState.isModelUpToDate())
        // {
        //     throw std::runtime_error("Model is not up to date");
        // }

        // if (!m_precompSdfIsValid)
        // {
        //     throw std::runtime_error("Precomputed SDF is not valid");
        // }

        glFinish();

        // if (!m_boundingBox.has_value())
        // {
        //     throw std::runtime_error("Bounding box is not valid");
        // }

        auto bb = getBoundingBox().value();
        if (std::isnan(bb.min.x) || std::isnan(bb.min.y) || std::isnan(bb.min.z) ||
            std::isnan(bb.max.x) || std::isnan(bb.max.y) || std::isnan(bb.max.z))
        {
            throw std::runtime_error("Bounding box is not valid");
        }

        auto backupEyePosition = m_resources->getEyePosition();
        auto backupViewPerspectiveMat = m_resources->getModelViewPerspectiveMat();

        ui::OrbitalCamera thumbnailCamera;
        const auto thumbnailSize = 256.0f;
        thumbnailCamera.centerView(bb);
        thumbnailCamera.setAngle(0.6f, -2.0f);

        thumbnailCamera.adjustDistanceToTarget(bb, thumbnailSize, thumbnailSize);

        thumbnailCamera.update(10000.f);

        applyCamera(thumbnailCamera);

        m_resources->getRenderingSettings().approximation = AM_FULL_MODEL;
        // getBestRenderProgram()->renderScene(
        //   *m_primitives, *m_thumbnailImageHighRes, 0, 0, m_thumbnailImageHighRes->getHeight());

        m_resources->setEyePosition(backupEyePosition);
        m_resources->setModelViewPerspectiveMat(backupViewPerspectiveMat);

        // getBestRenderProgram()->resample(
        //   *m_thumbnailImageHighRes, *m_thumbnailImage, 0, m_thumbnailImage->getHeight());

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

    PlainImage Rendering::createThumbnailPng()
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

    void Rendering::saveThumbnail(std::filesystem::path const & filename)
    {
        ProfileFunction std::lock_guard<std::recursive_mutex> lock(m_computeMutex);

        auto image = createThumbnail();

        // Save the image as a PNG file
        lodepng::encode(filename.string(),
                        image.data,
                        static_cast<unsigned int>(image.width),
                        static_cast<unsigned int>(image.height));
    }    void Rendering::applyCamera(ui::OrbitalCamera const & camera)
    {
        getResourceContext().setEyePosition(camera.getEyePosition());
        getResourceContext().setModelViewPerspectiveMat(camera.computeModelViewPerspectiveMatrix());
    }

    // void Rendering::injectSmoothingKernel(std::string const & kernel)
    // {
    //     if (!m_kernelReplacements)
    //     {
    //         m_kernelReplacements = std::make_shared<KernelReplacements>();
    //     }

    //     m_kernelReplacements->insert_or_assign("// <SMOOTHING KERNEL>", kernel);
    // }
}
