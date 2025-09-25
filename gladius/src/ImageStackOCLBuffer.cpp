#include "ImageStackOCLBuffer.h"
#include "ImageStackOCL.h"
#include <fmt/format.h>

namespace gladius
{
    // int getNumChannels(io::PixelFormat format)
    // {
    //     switch (format)
    //     {
    //     case io::PixelFormat::RGBA:
    //         return 4;
    //     case io::PixelFormat::RGB:
    //         return 3;
    //     case io::PixelFormat::GRAYSCALE:
    //         return 1;
    //     case io::PixelFormat::GRAYSCALE_ALPHA:
    //         return 2;
    //     default:
    //         throw std::runtime_error("ImageStackOCL::getNumChannels: unknown pixel format");
    //     }
    // }

    ImageStackOCLBuffer::ImageStackOCLBuffer(ComputeContext & context)
        : m_ComputeContext(context)
    {
    }

    void ImageStackOCLBuffer::initializeFromImageStack(io::ImageStack const & stack)
    {
        if (stack.empty())
        {
            throw std::runtime_error(
              "ImageStackOCLBuffer::initializeFromImageStack: stack is empty");
        }

        auto const & firstImage = stack.front();
        m_width = firstImage.getWidth();
        m_height = firstImage.getHeight();
        m_numSheets = stack.size();
        m_format = firstImage.getFormat();
        m_name = fmt::format("Image_{}", stack.getResourceId());

        m_numChannels = 4; // getNumChannels(m_format); lodepng seems to always return 4 channels

        // m_image3dRGBA =
        //   std::make_unique<Image3dRGBA>(m_ComputeContext, m_width, m_height, m_numSheets);

        // m_image3dRGBA->allocateOnDevice();

        m_buffer = std::make_unique<buffer3dRGBA>(m_ComputeContext);

        int sheetIndex = 0;
        auto & targetData = m_buffer->getData();
        targetData.clear();
        targetData.reserve(m_width * m_height * m_numSheets);

        for (auto const & image : stack)
        {
            auto const & data = image.getData();
            if (data.size() != m_width * m_height * m_numChannels)
            {
                throw std::runtime_error(
                  fmt::format("ImageStackOCLBuffer::initializeFromImageStack: image data size of "
                              "layer {} does not match image dimensions: {} != {} * {} * {} = {}",
                              sheetIndex,
                              data.size(),
                              m_width,
                              m_height,
                              m_numChannels,
                              m_width * m_height * m_numChannels));
            }

            for (size_t y = 0; y < m_height; ++y)
            {
                for (size_t x = 0; x < m_width; ++x)
                {
                    auto const index = (y * m_width + x) * m_numChannels;
                    auto const & r = data[index];
                    auto const & g = data[index + 1];
                    auto const & b = data[index + 2];
                    auto const & a = data[index + 3];

                    cl_float4 rgba = {r / 255.f, g / 255.f, b / 255.f, a / 255.f};
                    targetData.push_back(rgba);
                }
            }

            ++sheetIndex;
        }

        write();
    }

    cl::ImageFormat ImageStackOCLBuffer::getFormat() const
    {
        cl::ImageFormat format;
        switch (m_format)
        {
        case io::PixelFormat::RGBA_8BIT:
            format = {CL_RGBA, CL_FLOAT};
            break;
        case io::PixelFormat::RGB_8BIT:
            format = {CL_RGB, CL_FLOAT};
            break;
        case io::PixelFormat::GRAYSCALE_8BIT:
            format = {CL_R, CL_FLOAT};
            break;
        case io::PixelFormat::GRAYSCALE_ALPHA_8BIT:
            format = {CL_RA, CL_FLOAT};
            break;
        default:
            throw std::runtime_error("ImageStackOCLBuffer::getFormat: unknown pixel format");
        }
        return format;
    }

    void ImageStackOCLBuffer::write()
    {
        if (!m_buffer)
        {
            throw std::runtime_error("ImageStackOCLBuffer::write: m_buffer is null");
        }

        m_buffer->write();
        m_isUploaded = true;
    }

    std::string const & ImageStackOCLBuffer::getName() const
    {
        return m_name;
    }

    cl::Buffer const & ImageStackOCLBuffer::getBuffer() const
    {
        if (!m_isUploaded)
        {
            throw std::runtime_error("ImageStackOCLBuffer::getBuffer: image not uploaded");
        }

        if (!m_buffer)
        {
            throw std::runtime_error("ImageStackOCLBuffer::getBuffer: device buffer is null");
        }
        return m_buffer->getBuffer();
    }
}