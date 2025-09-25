#pragma once

#include "ComputeContext.h"
#include <CL/cl_platform.h>

namespace gladius
{
    class ImageBuffer
    {

      private:
        size_t m_width{};
        size_t m_height{};
        size_t m_depth{1};
        size_t m_size{};

        SharedComputeContext m_ComputeContext;
        std::unique_ptr < cl::Image
    }
}