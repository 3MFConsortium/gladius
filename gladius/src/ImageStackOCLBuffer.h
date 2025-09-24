#pragma once
#include "ComputeContext.h"

#include "Buffer.h"
#include "ResourceManager.h"
#include "io/3mf/ImageStack.h"
#include <CL/cl_platform.h>
#include <memory>

namespace gladius
{
    // int getNumChannels(io::PixelFormat format);
    using buffer3dRGBA = Buffer<cl_float4>;

    class ImageStackOCLBuffer
    {
      public:
        explicit ImageStackOCLBuffer(ComputeContext & context);

        void initializeFromImageStack(io::ImageStack const & stack);

        cl::ImageFormat getFormat() const;

        void allocateOnDevice();

        void write();

        std::string const & getName() const;

        cl::Buffer const & getBuffer() const;

      private:
        ComputeContext & m_ComputeContext;
        size_t m_width{};
        size_t m_height{};
        size_t m_numSheets{};
        size_t m_numChannels{};
        io::PixelFormat m_format{};
        std::string m_name;

        bool m_isUploaded{false};

        std::unique_ptr<buffer3dRGBA> m_buffer;
    };
}