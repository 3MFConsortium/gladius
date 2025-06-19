#pragma once

#include "GLImageBuffer.h"
#include "Primitives.h"
#include "ResourceContext.h"
#include "ProgramBase.h"

namespace gladius
{
    class RenderProgram : public ProgramBase
    {
      public:
        explicit RenderProgram(SharedComputeContext context, const SharedResources & resources);

        void renderScene(const Primitives & lines,
                         ImageRGBA & targetImage,
                         cl_float z_mm,
                         size_t startHeight,
                         size_t endHeight);

        void resample(ImageRGBA & sourceImage,
                      ImageRGBA & targetImage,
                      size_t startHeight,
                      size_t endHeight);

        bool  isSdfVisualizationEnabled() const;
        void setSdfVisualizationEnabled(bool value)
        {
            m_enableSdfVisualization = value;
            if (m_enableSdfVisualization)
            {
                m_programFront->addSymbol("RENDER_SDF");
            }
            else
            {
                m_programFront->removeSymbol("RENDER_SDF");
            }
        }
    private:
        bool m_enableSdfVisualization = true;
    };
}
