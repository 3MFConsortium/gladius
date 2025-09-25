#pragma once

#include "GLImageBuffer.h"
#include "Primitives.h"
#include "ProgramBase.h"
#include "ResourceContext.h"

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

      private:
    };
}
