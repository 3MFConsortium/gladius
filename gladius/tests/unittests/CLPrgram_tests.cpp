#include "CLProgram.h"
#include <gtest/gtest.h>

namespace gladius_tests
{
    using namespace gladius;

    TEST(CLProgramTest, ApplyKernelReplacements)
    {
        std::string source = "Hello, World!";
        KernelReplacements replacements = {{"Hello", "Hi"}, {"World", "Universe"}};

        applyKernelReplacements(source, replacements);

        EXPECT_EQ(source, "Hi, Universe!");
    }

    TEST(CLProgramTest, ApplyKernelReplacements_NoReplacements)
    {
        std::string source = "Hello, World!";
        KernelReplacements replacements;

        applyKernelReplacements(source, replacements);

        EXPECT_EQ(source, "Hello, World!");
    }

    TEST(CLProgramTest, ApplyKernelReplacements_MultipleReplacements)
    {
        std::string source = "Hello, World!";
        KernelReplacements replacements = {{"Hello", "Hi"}, {"World", "Universe"}, {"!", "?"}};

        applyKernelReplacements(source, replacements);

        EXPECT_EQ(source, "Hi, Universe?");
    }

}