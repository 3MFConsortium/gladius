#include "RenderProgram.h"
#include "Profiling.h"
#include "ProgramBase.h"
#include "gpgpu.h"

#include <CL/cl_platform.h>
#include <algorithm>

#include <cstddef>

namespace gladius
{
    RenderProgram::RenderProgram(SharedComputeContext context, const SharedResources & resources)
        : ProgramBase(context, resources)
    {
        m_sourceFiles = {"arguments.h",
                         "types.h",
                         "sdf.h",
                         "sampler.h",
                         "rendering.h",
                         "CNanoVDB.h",
                         "sdf.cl",
                         "rendering.cl",
                         "renderer.cl"};
    }

    void RenderProgram::renderScene(const Primitives & lines,
                                    ImageRGBA & targetImage,
                                    cl_float z_mm,
                                    size_t startHeight,
                                    size_t endHeight)
    {
        ProfileFunction;
        if (!m_programFront->isValid())
        {
            return;
        }
        swapProgramsIfNeeded();

        if (startHeight >= endHeight)
        {
            return;
        }
        auto const start = std::clamp(startHeight, size_t(0), targetImage.getHeight() - 2);
        auto const size =
          std::clamp(endHeight - startHeight, size_t{0}, targetImage.getHeight() - start - 1);

        if (size < 1 || size > 16000)
        {
            return;
        }
        cl::NDRange const origin = {0, start, 0};
        cl::NDRange const globalRange = {targetImage.getWidth(), size, 1};

        GLImageBuffer * glImageBuffer = dynamic_cast<GLImageBuffer *>(&targetImage);
        if (glImageBuffer)
        {
            glImageBuffer->invalidateContent();
        }

        m_resoures->getRenderingSettings().time_s = m_resoures->getTime_s();
        m_resoures->getRenderingSettings().z_mm = z_mm;

        try
        {
            if (!m_programFront->isValid())
            {
                return;
            }
            m_programFront->run("renderScene",
                                origin,
                                globalRange,
                                targetImage.getBuffer(),
                                m_resoures->getBuildArea(),
                                lines.primitives.getBuffer(),
                                cl_int(lines.primitives.getSize()),
                                lines.data.getBuffer(),
                                cl_int(lines.data.getSize()),
                                m_resoures->getRenderingSettings(),
                                m_resoures->getPrecompSdfBuffer().getBuffer(),
                                m_resoures->getParameterBuffer().getBuffer(),
                                m_resoures->getCommandBuffer().getBuffer(),
                                cl_int(m_resoures->getCommandBuffer().getData().size()),
                                m_resoures->getPreCompSdfBBox(),
                                // m_resoures->getImageStacks(),
                                m_resoures->getEyePosition(),
                                m_resoures->getModelViewPerspectiveMat());
        }
        catch (std::exception const & e)
        {
            if (m_logger)
            {
                m_logger->logError(std::string("RenderProgram error: ") + e.what());
            }
            else
            {
                std::cerr << e.what();
            }
        }
    }

    void RenderProgram::resample(ImageRGBA & sourceImage,
                                 ImageRGBA & targetImage,
                                 size_t startHeight,
                                 size_t endHeight)
    {
        ProfileFunction;
        swapProgramsIfNeeded();
        if (!m_programFront->isValid())
        {
            return;
        }
        cl::NDRange const origin = {0, startHeight, 0};
        cl::NDRange const range = {targetImage.getWidth(), endHeight, 1};
        m_programFront->run(
          "resample", origin, range, targetImage.getBuffer(), sourceImage.getBuffer());
    }

    // SDF visualization state is managed through ResourceContext::RenderingSettings.flags
}
