#include "BeamBVH.h"
#include "ResourceKey.h"
#include <gtest/gtest.h>

using namespace gladius;

namespace gladius::tests
{

    class BeamLatticeTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            // Setup test fixtures if needed
        }

        void TearDown() override
        {
            // Cleanup if needed
        }
    };

    /// @brief Test that headers can be included and compilation succeeds
    TEST_F(BeamLatticeTest, Headers_Include_CompilationSucceeds)
    {
        // This test validates that the beam lattice headers can be included
        // and that basic types are available for compilation
        EXPECT_TRUE(true); // This test passes if compilation succeeds
    }

    /// @brief Test ResourceKey functionality
    TEST_F(BeamLatticeTest, ResourceKey_Creation_Works)
    {
        ResourceKey testKey(123);
        EXPECT_TRUE(testKey.getResourceId().has_value());
        EXPECT_EQ(testKey.getResourceId().value(), 123);
    }

    /// @brief Test that BeamBVHBuilder type is available
    TEST_F(BeamLatticeTest, BeamBVHBuilder_TypeAvailable_CanDeclareVariable)
    {
        // Test that we can at least declare a variable of this type
        // This validates that the header is properly included and the type is accessible
        BeamBVHBuilder * builder = nullptr;
        EXPECT_EQ(builder, nullptr);
    }

} // namespace gladius::tests