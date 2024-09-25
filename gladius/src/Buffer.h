#pragma once

#include "ComputeContext.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace gladius
{
    template <typename T>
    class Buffer
    {
      public:
        explicit Buffer(ComputeContext & context)
            : m_ComputeContext(context)
        {
        }

        ~Buffer() = default;
        Buffer(Buffer const & other)
            : m_ComputeContext(other.m_ComputeContext)
            , m_data(other.m_data)
        {
            create();
        }

        // copying would require copying the buffer, which is not allowed
        Buffer(Buffer && other) = delete;
        
        Buffer & operator=(Buffer && other) = delete;
        Buffer & operator=(const Buffer & other) = delete;

        void read()
        {
            if (!m_buffer)
            {
                throw std::runtime_error("Failed to read, device buffer could not be created");
            }
            m_data.resize(m_size);
            CL_ERROR(m_ComputeContext.GetQueue().enqueueReadBuffer(
              *m_buffer.get(), CL_TRUE, 0, sizeof(T) * m_size, &m_data[0]));
            CL_ERROR(m_ComputeContext.GetQueue().finish());
        }

        
        void create()
        {
            if (m_data.empty())
            {
                m_data.push_back({});
            }
            cl_int error = 0;
            m_buffer = std::make_unique<cl::Buffer>(cl::Buffer(m_ComputeContext.GetContext(),
                                                               CL_MEM_READ_WRITE,
                                                               sizeof(T) * m_data.size(),
                                                               nullptr,
                                                               &error));
            CL_ERROR(error);
            m_size = m_data.size();
        }

        void clear()
        {
            m_data.clear();
            m_size = 0;
            m_buffer.reset();
        }

        void write()
        {
            if (m_data.empty())
            {
                return;
            }
            if (!m_buffer || m_size != m_data.size())
            {
                create(); // recreate buffer with the needed size
            }

            if (!m_buffer)
            {
                throw std::runtime_error("Failed to write, device buffer could not be created");
            }

            CL_ERROR(m_ComputeContext.GetQueue().enqueueWriteBuffer(
              *m_buffer.get(), CL_TRUE, 0, sizeof(T) * m_data.size(), &m_data[0]));

            CL_ERROR(m_ComputeContext.GetQueue().finish());
        }

        [[nodiscard]] std::vector<T> getDataCopy()
        {
            return m_data;
        }

        [[nodiscard]] std::vector<T> & getData()
        {
            return m_data;
        }

        [[nodiscard]] auto getSize() const -> size_t
        {
            return m_data.size();
        }

        void print() const
        {
            int elementCount = 0;
            int const lineBreak = static_cast<int>(std::sqrt(static_cast<double>(m_data.size())));

            for (const auto & res : m_data)
            {
                std::cout << res << " ";
                ++elementCount;
                if (elementCount == lineBreak)
                {
                    elementCount = 0;
                    std::cout << std::endl;
                }
            }
            std::cout << std::endl;
        }

        [[nodiscard]] auto getBuffer() const -> const cl::Buffer &
        {
            return *m_buffer.get();
        }

      private:
        std::vector<T> m_data;
        size_t m_size = 0;
        ComputeContext & m_ComputeContext;
        std::unique_ptr<cl::Buffer> m_buffer;
    };
}