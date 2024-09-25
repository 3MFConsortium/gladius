#pragma once
#include "ResourceKey.h"
#include <vector>

namespace gladius::io
{
    using ImageData = std::vector<unsigned char>;

    enum class PixelFormat
    {
        GRAYSCALE_1BIT,
        RGBA_8BIT,
        RGB_8BIT,
        GRAYSCALE_8BIT,
        GRAYSCALE_ALPHA_8BIT,
        RGBA_16BIT,
        RGB_16BIT,
        GRAYSCALE_16BIT,
        GRAYSCALE_ALPHA_16BIT
    };

    class Image
    {
      public:
        Image(ImageData const & data)
            : m_data(data)
        {
        }
        Image(ImageData const & data, unsigned int width, unsigned int height)
            : m_data(data)
            , m_width(width)
            , m_height(height)
        {
        }

        ImageData const & getData() const
        {
            return m_data;
        }

        unsigned int getWidth() const
        {
            return m_width;
        }

        unsigned int getHeight() const
        {
            return m_height;
        }

        PixelFormat getFormat() const
        {
            return m_format;
        }

        void setFormat(PixelFormat format)
        {
            m_format = format;
        }

        size_t getBitDepth() const
        {
            return m_bitDepth;
        }

        void setBitDepth(size_t bitDepth)
        {
            m_bitDepth = bitDepth;
        }

      private:
        ImageData m_data;
        unsigned int m_width{};
        unsigned int m_height{};
        PixelFormat m_format{};
        size_t m_bitDepth{8};
    };

    class ImageStack
    {
      public:
        ImageStack() = default;

        explicit ImageStack(ResourceId resourceId)
            : m_resourceId(resourceId)
        {
        }

        void setResourceId(ResourceId resourceId)
        {
            m_resourceId = resourceId;
        }

        ResourceId getResourceId() const
        {
            return m_resourceId;
        }

        auto begin() const
        {
            return m_stack.begin();
        }

        auto end() const
        {
            return m_stack.end();
        }

        auto size() const
        {
            return m_stack.size();
        }

        auto empty() const
        {
            return m_stack.empty();
        }

        auto front() const
        {
            return m_stack.front();
        }

        void push_back(Image const & image)
        {
            m_stack.push_back(image);
        }

        auto emplace_back(Image && image)
        {
            return m_stack.emplace_back(std::move(image));
        }

        void reserve(size_t size)
        {
            m_stack.reserve(size);
        }

      private:
        std::vector<Image> m_stack;
        ResourceId m_resourceId{};
    };
}