#pragma once

#include "ResourceManager.h"
#include "io/3mf/ImageStack.h"

namespace gladius
{
    class ImageStackResource : public ResourceBase
    {
      public:
        explicit ImageStackResource(ResourceKey key, io::ImageStack && stack);

        ~ImageStackResource() = default;

        size_t getWidth() const
        {
            return m_width;
        }

        size_t getHeight() const
        {
            return m_height;
        }

        size_t getNumSheets() const
        {
            return m_numSheets;
        }

        size_t getNumChannels() const
        {
            return m_numChannels;
        }

      private:
        size_t m_width{};
        size_t m_height{};
        size_t m_numSheets{};
        size_t m_numChannels{};
        io::PixelFormat m_format{};

        io::ImageStack m_stack;

        void loadImpl() override;
    };
}
