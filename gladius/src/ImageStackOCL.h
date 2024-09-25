#pragma once
#include "ComputeContext.h"

#include "ImageRGBA.h"
#include "io/3mf/ImageStack.h"
#include <CL/cl_platform.h>
#include <memory>

namespace gladius
{
    int getNumChannels(io::PixelFormat format);
    using Image3dRGBA = ImageImpl<cl_float4>;

    class ImageStackOCL
    {
      public:
        explicit ImageStackOCL(ComputeContext & context);

        void initializeFromImageStack(io::ImageStack const & stack);

        cl::ImageFormat getFormat() const;

        void allocateOnDevice();

        void write();

        std::string const & getName() const;

        cl::Image & getBuffer() const;

      private:
        ComputeContext & m_ComputeContext;
        size_t m_width{};
        size_t m_height{};
        size_t m_numSheets{};
        size_t m_numChannels{};
        io::PixelFormat m_format{};
        std::string m_name;

        bool m_isUploaded{false};

        std::unique_ptr<Image3dRGBA> m_image3dRGBA;
    };
}