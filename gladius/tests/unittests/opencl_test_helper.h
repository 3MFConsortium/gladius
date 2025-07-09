#pragma once

#include <ComputeContext.h>
#include <gtest/gtest.h>

/// @brief Macro to skip tests that require OpenCL if it's not available
/// This should be used at the beginning of any test that creates a ComputeContext or ComputeCore
#define SKIP_IF_OPENCL_UNAVAILABLE()                                                               \
    do                                                                                             \
    {                                                                                              \
        if (!gladius::ComputeContext::isOpenCLAvailable())                                         \
        {                                                                                          \
            GTEST_SKIP() << "OpenCL is not available on this system";                              \
        }                                                                                          \
    } while (0)
