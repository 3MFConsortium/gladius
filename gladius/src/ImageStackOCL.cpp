#include "ImageStackOCL.h"
#include <fmt/format.h>

namespace gladius
{
    int getNumChannels(io::PixelFormat format)
    {
        switch (format)
        {
        case io::PixelFormat::RGBA_16BIT:
        case io::PixelFormat::RGBA_8BIT:
            return 4;
        case io::PixelFormat::RGB_16BIT:
        case io::PixelFormat::RGB_8BIT:
            return 3;
        case io::PixelFormat::GRAYSCALE_16BIT:
        case io::PixelFormat::GRAYSCALE_8BIT:
            return 1;
        case io::PixelFormat::GRAYSCALE_ALPHA_16BIT:
        case io::PixelFormat::GRAYSCALE_ALPHA_8BIT:
            return 2;
        default:
            throw std::runtime_error("ImageStackOCL::getNumChannels: unknown pixel format");
        }
    }

    ImageStackOCL::ImageStackOCL(ComputeContext & context)
        : m_ComputeContext(context)
    {
    }

    void ImageStackOCL::initializeFromImageStack(io::ImageStack const & stack)
    {
        if (stack.empty())
        {
            throw std::runtime_error("ImageStackOCL::initializeFromImageStack: stack is empty");
        }

        auto const & firstImage = stack.front();
        m_width = firstImage.getWidth();
        m_height = firstImage.getHeight();
        m_numSheets = stack.size();
        m_format = firstImage.getFormat();
        m_name = fmt::format("Image_{}", stack.getResourceId());

        m_numChannels = 4; // getNumChannels(m_format); lodepng seems to always return 4 channels

        m_image3dRGBA =
          std::make_unique<Image3dRGBA>(m_ComputeContext, m_width, m_height, m_numSheets);

        m_image3dRGBA->allocateOnDevice();

        int i = 0;

        for (auto const & image : stack)
        {
            auto const & data = image.getData();
            if (data.size() != m_width * m_height * m_numChannels)
            {
                throw std::runtime_error(
                  fmt::format("ImageStackOCL::initializeFromImageStack: image data size of "
                              "layer {} does not match image dimensions: {} != {} * {} * {} = {}",
                              i,
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
                    // cl_float4 const rgba = {static_cast<float>(x) / static_cast<float>(m_width),
                    //                         static_cast<float>(y) / static_cast<float>(m_height),
                    //                         static_cast<float>(i) /
                    //                         static_cast<float>(m_numSheets), 1.f};
                    m_image3dRGBA->setValue(x, y, i, rgba);
                }
            }

            ++i;
        }

        write();
    }

    cl::ImageFormat ImageStackOCL::getFormat() const
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
            throw std::runtime_error("ImageStackOCL::getFormat: unknown pixel format");
        }
        return format;
    }

    void ImageStackOCL::write()
    {
        if (!m_image3dRGBA)
        {
            throw std::runtime_error("ImageStackOCL::write: image3dRGBA is null");
        }

        m_image3dRGBA->write();
        m_isUploaded = true;
    }

    std::string const & ImageStackOCL::getName() const
    {
        return m_name;
    }

    cl::Image & ImageStackOCL::getBuffer() const
    {
        if (!m_isUploaded)
        {
            throw std::runtime_error("ImageStackOCL::getBuffer: image not uploaded");
        }

        if (!m_image3dRGBA)
        {
            throw std::runtime_error("ImageStackOCL::getBuffer: device buffer is null");
        }
        return m_image3dRGBA->getBuffer();
    }
}