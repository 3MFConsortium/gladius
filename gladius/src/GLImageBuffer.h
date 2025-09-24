
#pragma once

#include "ComputeContext.h"

#include "ImageRGBA.h"

#include <iostream>

namespace gladius
{
    class GLImageBuffer final : public ImageRGBA
    {
      public:
        explicit GLImageBuffer(ComputeContext & context)
            : ImageRGBA(context)
        {
        }

        GLImageBuffer(ComputeContext & context, size_t width, size_t height)
            : ImageRGBA(context, width, height)
        {
        }

        ~GLImageBuffer() override;

        void allocateOnDevice() override;

        void bind();

        static void unbind();

        [[nodiscard]] GLuint GetTextureId() const;

        void transferPixelInReadPixelMode();

        void invalidateContent();

      private:
        void setupForInterOp();
        void setupForReadPixel();

        void transferPixels();

        GLuint m_textureID = 0;
        bool m_dirty{true};
    };
}
