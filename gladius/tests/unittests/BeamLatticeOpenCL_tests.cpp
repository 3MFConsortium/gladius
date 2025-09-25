/// @file BeamLatticeOpenCL_tests.cpp
/// @brief Minimal OpenCL unit tests for beam lattice functionality using Google Test
/// @author AI Assistant
/// @date 2025

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#ifdef ENABLE_OPENCL_TESTS

#include "CLProgram.h"
#include "ComputeContext.h"
#include "kernel/types.h"

#include <memory>

using namespace gladius;
using namespace testing;

namespace gladius_tests
{
    /// @brief Test fixture for OpenCL beam lattice tests
    class BeamLatticeOpenCLTest : public Test
    {
      protected:
        void SetUp() override
        {
            // Initialize OpenCL context
            try
            {
                m_context = std::make_unique<ComputeContext>();
                m_context->initialize();
            }
            catch (const std::exception & e)
            {
                GTEST_SKIP() << "OpenCL not available or failed to initialize: " << e.what();
            }
        }

        void TearDown() override
        {
            // Clean up OpenCL resources
            m_context.reset();
        }

      protected:
        std::unique_ptr<ComputeContext> m_context;
    };

    // ============================================================================
    // OpenCL Context Tests
    // ============================================================================

    TEST_F(BeamLatticeOpenCLTest, OpenCLContext_Initialization_Succeeds)
    {
        EXPECT_NE(m_context, nullptr) << "ComputeContext should be initialized";
        if (m_context)
        {
            EXPECT_TRUE(m_context->isInitialized())
              << "ComputeContext should be properly initialized";
        }
    }

    TEST_F(BeamLatticeOpenCLTest, OpenCLContext_DeviceInfo_Available)
    {
        if (!m_context)
            return;

        // Basic test to ensure we can query device information
        auto devices = m_context->getDevices();
        EXPECT_GT(devices.size(), 0u) << "Should have at least one OpenCL device";
    }

} // namespace gladius_tests

#endif // ENABLE_OPENCL_TESTS
