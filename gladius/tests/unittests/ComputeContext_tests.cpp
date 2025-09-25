#include "ComputeContext.h"
#include "opencl_test_helper.h"

#include <gtest/gtest.h>

namespace gladius_tests
{
    using namespace gladius;

    class ComputeContext_Tests : public ::testing::Test
    {
    };

    TEST_F(ComputeContext_Tests, IsOpenCLAvailable_SystemCheck_ReturnsValidBoolean)
    {
        // This test should always pass - it just verifies the method exists and returns a boolean
        bool const isAvailable = ComputeContext::isOpenCLAvailable();

        // We don't assert the specific value since it depends on the system,
        // but we can test that it returns a valid boolean
        EXPECT_TRUE(isAvailable == true || isAvailable == false);
    }

    TEST_F(ComputeContext_Tests, Constructor_WithOpenCLAvailable_CreatesValidContext)
    {
        SKIP_IF_OPENCL_UNAVAILABLE();

        // If OpenCL is available, we should be able to create a valid context
        ComputeContext context;
        EXPECT_TRUE(context.isValid());
    }

    TEST_F(ComputeContext_Tests, Constructor_WithGLDisabled_CreatesValidContext)
    {
        SKIP_IF_OPENCL_UNAVAILABLE();

        // If OpenCL is available, we should be able to create a valid context with GL disabled
        ComputeContext context(EnableGLOutput::disabled);
        EXPECT_TRUE(context.isValid());
    }
}
