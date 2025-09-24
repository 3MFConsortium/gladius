#pragma once

#include "ComputeContext.h"
#include <CL/cl_platform.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace gladius
{
    template <typename ImageDataPoint>
    class ImageImpl
    {
      public:
        explicit ImageImpl(ComputeContext & context)
            : m_width(512)
            , m_height(512)
            , m_size(m_width * m_height * m_depth)
            , m_ComputeContext(context)
        {
        }

        ImageImpl(ComputeContext & ComputeContext, size_t width, size_t height)
            : m_width(width)
            , m_height(height)
            , m_size(m_width * m_height * m_depth)
            , m_ComputeContext(ComputeContext)
        {
        }

        ImageImpl(ComputeContext & ComputeContext, size_t width, size_t height, size_t depth)
            : m_width(width)
            , m_height(height)
            , m_depth(depth)
            , m_size(m_width * m_height * m_depth)
            , m_ComputeContext(ComputeContext)
        {
        }

        // Note that only the content of the device buffer is copied but not download to the host
        ImageImpl(ImageImpl<ImageDataPoint> & src)
            : m_width(src.getWidth())
            , m_height(src.getHeight())
            , m_size(m_width * m_height)
            , m_ComputeContext(src.m_ComputeContext)
        {
            ImageImpl<ImageDataPoint>::allocateOnDevice();

            CL_ERROR(m_ComputeContext.GetQueue().enqueueCopyImage(
              src.getBuffer(), getBuffer(), {0, 0, 0}, {0, 0, 0}, {m_width, m_height, m_depth}));
            CL_ERROR(m_ComputeContext.GetQueue().finish());
        }

        virtual ~ImageImpl()
        {
            if (m_buffer && m_allocatedBytes > 0)
            {
                m_ComputeContext.onBufferReleased(m_allocatedBytes);
                m_allocatedBytes = 0;
            }
        }

        void setWidth(size_t width)
        {

            m_width = std::max(static_cast<size_t>(1), width);
        }

        void setHeight(size_t height)
        {
            m_height = std::max(static_cast<size_t>(1), height);
        }

        void read()
        {
            CL_ERROR(m_ComputeContext.GetQueue().finish());
            CL_ERROR(m_ComputeContext.GetQueue().enqueueReadImage(
              *m_buffer, CL_TRUE, {0, 0, 0}, {m_width, m_height, m_depth}, 0, 0, m_data.data()));
            CL_ERROR(m_ComputeContext.GetQueue().finish());
        }

        void write()
        {
            CL_ERROR(m_ComputeContext.GetQueue().finish());
            CL_ERROR(m_ComputeContext.GetQueue().enqueueWriteImage(
              *m_buffer, CL_TRUE, {0, 0, 0}, {m_width, m_height, m_depth}, 0, 0, m_data.data()));
            CL_ERROR(m_ComputeContext.GetQueue().finish());
        }

        void fill(ImageDataPoint value)
        {
            std::fill(std::begin(m_data), std::end(m_data), value);
        }

        virtual void allocateOnDevice()
        {
            const cl::ImageFormat format = determineImageFormat();

            m_data.resize(m_width * m_height * m_depth);

            // If re-allocating, release previous accounting tracked
            if (m_buffer && m_allocatedBytes > 0)
            {
                m_ComputeContext.onBufferReleased(m_allocatedBytes);
                m_allocatedBytes = 0;
            }

            if (m_depth == 1)
            {
                m_buffer = m_ComputeContext.createImage2DChecked(format,
                                                                 m_width,
                                                                 m_height,
                                                                 CL_MEM_READ_WRITE,
                                                                 0,
                                                                 nullptr,
                                                                 typeid(ImageDataPoint).name());
                m_allocatedBytes =
                  ComputeContext::estimateImageSizeBytes(format, m_width, m_height, 1);
            }
            else
            {
                m_buffer = m_ComputeContext.createImage3DChecked(format,
                                                                 m_width,
                                                                 m_height,
                                                                 m_depth,
                                                                 CL_MEM_READ_WRITE,
                                                                 0,
                                                                 0,
                                                                 nullptr,
                                                                 typeid(ImageDataPoint).name());
                m_allocatedBytes =
                  ComputeContext::estimateImageSizeBytes(format, m_width, m_height, m_depth);
            }
            write();
        }

        std::vector<ImageDataPoint> & getData()
        {
            return m_data;
        }

        [[nodiscard]] const std::vector<ImageDataPoint> & getData() const
        {
            return m_data;
        }

        void print()
        {
            int i = 0;

            for (auto res : m_data)
            {
                std::cout << res.x << " ";
                ++i;

                if (i == static_cast<int>(sqrt((double) m_data.size())))
                {
                    i = 0;
                    std::cout << std::endl;
                }
            }

            std::cout << std::endl;
        }

        [[nodiscard]] size_t index(size_t x, size_t y) const
        {
            const auto ix = std::clamp(x, static_cast<size_t>(3), m_width - 2);
            const auto iy = std::clamp(y, static_cast<size_t>(3), m_height - 2);
            return iy * m_width + ix;
        }

        [[nodiscard]] size_t index(size_t x, size_t y, size_t z) const
        {
            const auto ix = std::clamp(x, static_cast<size_t>(0), m_width);
            const auto iy = std::clamp(y, static_cast<size_t>(0), m_height);
            const auto iz = std::clamp(z, static_cast<size_t>(0), m_depth);

            return std::clamp(
              iz * m_width * m_height + iy * m_width + ix, static_cast<size_t>(0), m_size - 1);
        }

        cl::Image & getBuffer()
        {
            return *m_buffer;
        }

        cl::Image * getBufferPtr()
        {
            return m_buffer.get();
        }

        [[nodiscard]] size_t getWidth() const
        {
            return m_width;
        }

        [[nodiscard]] size_t getHeight() const
        {
            return m_height;
        }

        [[nodiscard]] size_t getDepth() const
        {
            return m_depth;
        }

        [[nodiscard]] ImageDataPoint getValue(size_t x, size_t y) const
        {
            auto id = index(x, y);
            return m_data[id];
        }

        [[nodiscard]] ImageDataPoint getValue(size_t x, size_t y, size_t z) const
        {
            auto id = index(x, y, z);
            return m_data[id];
        }

        void setValue(size_t x, size_t y, ImageDataPoint value)
        {
            auto id = index(x, y);
            m_data[id] = value;
        }

        void setValue(size_t x, size_t y, size_t z, ImageDataPoint value)
        {
            auto id = index(x, y, z);
            m_data[id] = value;
        }

      protected:
        std::vector<ImageDataPoint> m_data;

        size_t m_width;
        size_t m_height;
        size_t m_depth = 1;

        size_t m_size = 0;
        ComputeContext & m_ComputeContext;
        std::unique_ptr<cl::Image> m_buffer;

        // Track bytes accounted in ComputeContext for this device image
        size_t m_allocatedBytes{0};

        static cl::ImageFormat determineImageFormat()
        {
            if constexpr (std::is_same_v<ImageDataPoint, cl_int>)
            {
                return cl::ImageFormat{CL_R, CL_SIGNED_INT32};
            }
            else if constexpr (std::is_same_v<ImageDataPoint, cl_int2>)
            {
                return cl::ImageFormat{CL_RG, CL_SIGNED_INT32};
            }
            else if constexpr (std::is_same_v<ImageDataPoint, cl_float2>)
            {
                return cl::ImageFormat{CL_RG, CL_FLOAT};
            }
            else if constexpr (std::is_same_v<ImageDataPoint, cl_float4>)
            {
                return cl::ImageFormat{CL_RGBA, CL_FLOAT};
            }
            else if constexpr (std::is_same_v<ImageDataPoint, cl_uchar>)
            {
                return cl::ImageFormat{CL_R, CL_UNSIGNED_INT8};
            }
            else if constexpr (std::is_same_v<ImageDataPoint, cl_float>)
            {
                return cl::ImageFormat{CL_R, CL_FLOAT};
            }
            else if constexpr (std::is_same_v<ImageDataPoint, cl_char4>)
            {
                return cl::ImageFormat{CL_RGBA, CL_UNSIGNED_INT8};
            }
            else if constexpr (std::is_same_v<ImageDataPoint, cl_char>)
            {
                return cl::ImageFormat{CL_R, CL_UNSIGNED_INT8};
            }
            else
            {
                throw std::domain_error("Image Format is not supported!");
            }
        }
    };

    using ImageRGBA = ImageImpl<cl_float4>;

    // TODO: Replace cl_floatN by structs if applicable
    // x = euclidean distance, y == 0:  does not need to be evaluate in the next layer, y ==
    // FLT_MAX:
    using DistanceMap = ImageImpl<cl_float2>;
    using DepthBuffer = ImageImpl<cl_float>;

    using PowerMap = ImageImpl<cl_float2>;
    // x and y are the position, z is set to FLT_MAX if the vertex is not contained in a contour
    using Vertices = ImageImpl<cl_float4>;

    using Normals = ImageImpl<cl_float2>;   // aka gradient
    using Adjacencies = ImageImpl<cl_int2>; // coordinate indices
    using JfAMap = ImageImpl<cl_float2>;    // normalized coordinates

    using Skeleton = ImageImpl<cl_int>;

    using PreComputedSdf = ImageImpl<cl_float>;

    using MarchingSquaresStates = ImageImpl<cl_char>;
}
