#include "ModelState.h"

#include <gtest/gtest.h>

namespace gladius_tests
{

    class ModelState_Tests : public ::testing::Test
    {
    };

    TEST_F(ModelState_Tests, IsCompilationRequired_NoCompilationRequested_ReturnsFalse)
    {
        gladius::ModelState const modelState;

        EXPECT_FALSE(modelState.isCompilationRequired());
        EXPECT_TRUE(modelState.isModelUpToDate());
    }

    TEST_F(ModelState_Tests, IsCompilationRequired_CompilationRequested_ReturnsTrue)
    {
        gladius::ModelState modelState;

        modelState.signalCompilationRequired();
        EXPECT_TRUE(modelState.isCompilationRequired());
    }

    TEST_F(ModelState_Tests, IsCompilationRequired_CompilationFinished_ReturnsFalse)
    {
        gladius::ModelState modelState;

        modelState.signalCompilationRequired();
        modelState.signalCompilationStarted();
        modelState.signalCompilationFinished();
        EXPECT_FALSE(modelState.isCompilationRequired());
    }

    TEST_F(ModelState_Tests, IsCompilationRequired_CompilationStarted_ReturnsFalse)
    {
        gladius::ModelState modelState;

        modelState.signalCompilationRequired();
        modelState.signalCompilationStarted();
        EXPECT_FALSE(modelState.isCompilationRequired());
        EXPECT_FALSE(modelState.isModelUpToDate());
    }

    TEST_F(ModelState_Tests, IsCompilationRequired_CompilationRequestWhileInProgress_ReturnsTrue)
    {
        gladius::ModelState modelState;

        modelState.signalCompilationRequired();
        modelState.signalCompilationStarted();
        modelState.signalCompilationRequired();
        EXPECT_TRUE(modelState.isCompilationRequired());
        EXPECT_FALSE(modelState.isModelUpToDate());
    }

    TEST_F(ModelState_Tests, IsCompilationRequired_CompilationFinishedButNewRequestWhileInProgress_ReturnsTrue)
    {
        gladius::ModelState modelState;

        modelState.signalCompilationRequired();
        modelState.signalCompilationStarted();
        modelState.signalCompilationRequired();
        modelState.signalCompilationFinished(); 

        // In ths case a new compilation is required to consider the second compilation request
        EXPECT_TRUE(modelState.isCompilationRequired());
        EXPECT_FALSE(modelState.isModelUpToDate());
    }
}