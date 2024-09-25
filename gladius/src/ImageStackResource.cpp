#include "ImageStackResource.h"

namespace gladius
{
    ImageStackResource::ImageStackResource(ResourceKey key, io::ImageStack && stack)
        : ResourceBase(std::move(key))
        , m_stack(std::move(stack))
    {
        ResourceBase::load();
    }

    size_t numChannelsFromPixelFormat(io::PixelFormat format)
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
            throw std::runtime_error("numChannelsFromPixelFormat: unknown pixel format");
        }
    }

    void ImageStackResource::loadImpl()
    {
        if (m_stack.empty())
        {
            return; // throw std::runtime_error("ImageStackResource::loadImpl: stack is empty");
        }

        m_payloadData.meta.clear();

        PrimitiveMeta metaData{};
        metaData.primitiveType = SDF_IMAGESTACK;
        metaData.start = static_cast<int>(m_payloadData.data.size());

        auto const & firstImage = m_stack.front();
        m_width = firstImage.getWidth();
        m_height = firstImage.getHeight();
        m_numSheets = m_stack.size();
        m_format = firstImage.getFormat();
        m_numChannels = numChannelsFromPixelFormat(m_format);

        int sheetIndex = 0;
        for (auto const & image : m_stack)
        {
            auto const & data = image.getData();
            if (data.size() != m_width * m_height * 4)
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

            if (m_height < 1)
            {
                throw std::runtime_error(
                  fmt::format("ImageStackOCLBuffer::initializeFromImageStack: image height is less "
                              "than 1: {}",
                              m_height));
            }

            for (size_t y = m_height; y > 0u; --y)
            {
                for (size_t x = 0; x < m_width; ++x)
                {
                    auto const index = ((y - 1) * m_width + x) * 4;

                    auto const & r = data[index];
                    m_payloadData.data.push_back(r / 255.f);

                    switch (m_format)
                    {
                    case io::PixelFormat::RGBA_16BIT:
                    case io::PixelFormat::RGBA_8BIT:
                    {
                        auto const & g = data[index + 1];
                        auto const & b = data[index + 2];
                        auto const & a = data[index + 3];

                        m_payloadData.data.push_back(g / 255.f);
                        m_payloadData.data.push_back(b / 255.f);
                        m_payloadData.data.push_back(a / 255.f);
                        break;
                    }
                    case io::PixelFormat::RGB_16BIT:
                    case io::PixelFormat::RGB_8BIT:
                    {
                        auto const & g = data[index + 1];
                        auto const & b = data[index + 2];

                        m_payloadData.data.push_back(g / 255.f);
                        m_payloadData.data.push_back(b / 255.f);
                        m_payloadData.data.push_back(1.f);
                        break;
                    }
                    case io::PixelFormat::GRAYSCALE_16BIT:
                    case io::PixelFormat::GRAYSCALE_8BIT:
                    {
                        m_payloadData.data.push_back(r / 255.f);
                        m_payloadData.data.push_back(r / 255.f);
                        m_payloadData.data.push_back(1.f);
                        break;
                    }
                    case io::PixelFormat::GRAYSCALE_ALPHA_16BIT:
                    case io::PixelFormat::GRAYSCALE_ALPHA_8BIT:
                    {
                        m_payloadData.data.push_back(r / 255.f);
                        m_payloadData.data.push_back(r / 255.f);

                        auto const & a = data[index + 1];
                        m_payloadData.data.push_back(a / 255.f);
                        break;
                    }
                    case io::PixelFormat::GRAYSCALE_1BIT:
                    {
                        m_payloadData.data.push_back(r > 0 ? 1.f : 0.f);
                        m_payloadData.data.push_back(r > 0 ? 1.f : 0.f);
                        m_payloadData.data.push_back(r > 0 ? 1.f : 0.f);
                        break;
                    }
                    default:
                        throw std::runtime_error(
                          "ImageStackResource::loadImpl: unknown pixel format");
                    }
                }
            }

            ++sheetIndex;
        }
        metaData.end = static_cast<int>(m_payloadData.data.size());
        m_payloadData.meta.push_back(metaData);
    }
}