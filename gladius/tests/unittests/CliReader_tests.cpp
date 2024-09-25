

#include <CliReader.h>

#include <gtest/gtest.h>

namespace gladius_tests
{
    TEST(CliReader, Read_CliWithQuad_PrimitivesContainQuad)
    {
        gladius::CliReader testee;
        gladius::ComputeContext computecontext{};
        gladius::Primitives result{computecontext};

        testee.read({"testdata/quad.cli"}, result);
        EXPECT_EQ(1, result.primitives.getSize());
        EXPECT_EQ(8, result.data.getData().size());
    }
}