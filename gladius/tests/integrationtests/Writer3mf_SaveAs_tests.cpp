/**
 * @file Writer3mf_SaveAs_tests_new.cpp
 * @brief Integration tests for Writer3mf saveAs operations
 *
 * This test file is designed to reproduce and debug save operation failures.
 * The tests use valid input but should initially fail to help identify
 * the root cause of save operation issues.
 */

#include "testdata.h"
#include "testhelper.h"

#include "ComputeContext.h"
#include "Document.h"
#include "EventLogger.h"
#include "ExpressionParser.h"
#include "ExpressionToGraphConverter.h"
#include "compute/ComputeCore.h"
#include "compute/types.h"
#include "nodes/Assembly.h"
#include "nodes/Model.h"

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <string>

namespace gladius_integration_tests
{
    /**
     * @brief Integration test class for Writer3mf save operations
     *
     * These tests create real documents with mathematical functions and attempt
     * to save them to 3MF files. They are designed to help debug save operation
     * failures by providing comprehensive logging throughout the save pipeline.
     */
    class Writer3mfSaveAsTest : public ::testing::Test
    {
      protected:
        std::shared_ptr<gladius::events::Logger> m_logger;
        std::shared_ptr<gladius::ComputeCore> m_core;
        std::shared_ptr<gladius::Document> m_document;
        std::filesystem::path m_tempDir;

        void SetUp() override
        {
            Test::SetUp();

            // Initialize logger for debugging
            m_logger =
              std::make_shared<gladius::events::Logger>(gladius::events::OutputMode::Console);

            // Initialize compute context and core for document
            try
            {
                auto context = std::make_shared<gladius::ComputeContext>();
                if (!context->isValid())
                {
                    GTEST_SKIP()
                      << "Failed to create compute context - GPU drivers may not be available";
                }

                m_core = std::make_shared<gladius::ComputeCore>(
                  context, gladius::RequiredCapabilities::ComputeOnly, m_logger);
                m_document = std::make_shared<gladius::Document>(m_core);
            }
            catch (const std::exception & e)
            {
                GTEST_SKIP() << "Failed to initialize compute core: " << e.what();
            }

            // Create temporary directory for test files
            m_tempDir = std::filesystem::temp_directory_path() /
                        ("gladius_writer3mf_tests_" +
                         std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::system_clock::now().time_since_epoch())
                                          .count()));
            std::filesystem::create_directories(m_tempDir);

            m_logger->logInfo("Writer3mf integration test setup completed");
            m_logger->logInfo("Temp directory: " + m_tempDir.string());
        }

        void TearDown() override
        {
            // Clean up test files
            if (std::filesystem::exists(m_tempDir))
            {
                try
                {
                    std::filesystem::remove_all(m_tempDir);
                    m_logger->logInfo("Cleaned up temp directory: " + m_tempDir.string());
                }
                catch (const std::exception & e)
                {
                    m_logger->logWarning("Failed to clean up temp directory: " +
                                         std::string(e.what()));
                }
            }

            m_document.reset();
            m_core.reset();
            m_logger.reset();
        }

        /**
         * @brief Create a gyroid function in the document
         * @return ResourceId of the created function
         */
        gladius::ResourceId createGyroidFunction()
        {
            m_logger->logInfo("Creating gyroid function...");

            // Get the document's assembly
            auto assembly = m_document->getAssembly();
            if (!assembly)
            {
                m_logger->logError("Failed to get assembly from document");
                return 0; // Invalid ResourceId
            }

            // Get the active model
            auto model = assembly->assemblyModel();
            if (!model)
            {
                m_logger->logError("Failed to get assembly model from assembly");
                return 0; // Invalid ResourceId
            }

            // Create gyroid expression: sin(x)*cos(y) + sin(y)*cos(z) + sin(z)*cos(x)
            std::string gyroidExpression = "sin(x)*cos(y) + sin(y)*cos(z) + sin(z)*cos(x)";
            m_logger->logInfo("Gyroid expression: " + gyroidExpression);

            try
            {
                // Parse the expression
                gladius::ExpressionParser parser;
                if (!parser.parseExpression(gyroidExpression))
                {
                    m_logger->logError("Failed to parse expression: " + parser.getLastError());
                    return 0; // Invalid ResourceId
                }
                m_logger->logInfo("Expression parsed successfully");

                // Convert to graph
                gladius::ExpressionToGraphConverter converter;
                auto nodeId = converter.convertExpressionToGraph(gyroidExpression, *model, parser);
                if (nodeId == 0)
                {
                    m_logger->logError("Failed to convert expression to graph");
                    return 0; // Invalid ResourceId
                }
                m_logger->logInfo("Expression converted to graph successfully");

                // For this test, we'll return a valid ResourceId (assuming the conversion worked)
                // In a real implementation, this would come from the model's function management
                return 1; // Placeholder - actual implementation would return proper ID
            }
            catch (const std::exception & e)
            {
                m_logger->logError("Exception creating gyroid function: " + std::string(e.what()));
                return 0; // Invalid ResourceId
            }
        }

        /**
         * @brief Get test file path in temp directory
         */
        std::filesystem::path getTestFilePath(const std::string & filename) const
        {
            return m_tempDir / filename;
        }

        /**
         * @brief Check if file exists and has reasonable size
         */
        bool validateSavedFile(const std::filesystem::path & filepath) const
        {
            if (!std::filesystem::exists(filepath))
            {
                m_logger->logError("File does not exist: " + filepath.string());
                return false;
            }

            auto fileSize = std::filesystem::file_size(filepath);
            m_logger->logInfo("File size: " + std::to_string(fileSize) + " bytes");

            if (fileSize == 0)
            {
                m_logger->logError("File is empty: " + filepath.string());
                return false;
            }

            // 3MF files should be at least a few KB (basic zip structure + minimal content)
            if (fileSize < 1024)
            {
                m_logger->logWarning("File size suspiciously small for 3MF file: " +
                                     std::to_string(fileSize) + " bytes");
                return false;
            }

            m_logger->logInfo("File validation passed: " + filepath.string());
            return true;
        }
    };

    /**
     * @brief Test saving a document with a gyroid function
     *
     * This test creates a valid document with a gyroid function and attempts to save it.
     * It should initially fail to help identify the root cause of save operation issues.
     */
    TEST_F(Writer3mfSaveAsTest, SaveGyroidFunction_ValidDocument_ExpectedToSaveSuccessfully)
    {
        m_logger->logInfo("=== Starting SaveGyroidFunction test ===");

        // Create a gyroid function in the document
        auto functionId = createGyroidFunction();
        ASSERT_NE(functionId, 0) << "Failed to create gyroid function for testing";

        // Attempt to save the document
        auto testFile = getTestFilePath("gyroid_function_test.3mf");
        m_logger->logInfo("Attempting to save document to: " + testFile.string());

        try
        {
            m_document->saveAs(testFile, true); // true = write thumbnail
            m_logger->logInfo("Document.saveAs() completed without throwing exception");

            // Check if file was created and is valid
            bool isValid = validateSavedFile(testFile);

            // For debugging purposes, we expect this to fail initially
            // Once the bug is fixed, this assertion should be changed to EXPECT_TRUE
            EXPECT_TRUE(isValid) << "Expected save operation to fail for debugging purposes - "
                                 << "if this passes, the bug may already be fixed!";
        }
        catch (const std::exception & e)
        {
            m_logger->logError("Exception during save operation: " + std::string(e.what()));
            FAIL() << "Save operation threw exception: " << e.what();
        }

        m_logger->logInfo("=== SaveGyroidFunction test completed ===");
    }

    /**
     * @brief Test saving an empty document
     *
     * This test attempts to save an empty document to identify if the issue
     * is related to having functions in the document.
     */
    TEST_F(Writer3mfSaveAsTest, SaveEmptyDocument_NoFunctions_ShouldProvideBaselineBehavior)
    {
        m_logger->logInfo("=== Starting SaveEmptyDocument test ===");

        auto testFile = getTestFilePath("empty_document_test.3mf");
        m_logger->logInfo("Attempting to save empty document to: " + testFile.string());

        try
        {
            m_document->saveAs(testFile, true);
            m_logger->logInfo("Empty document save completed without throwing exception");

            bool isValid = validateSavedFile(testFile);
            EXPECT_TRUE(isValid)
              << "Empty document save should succeed and provide baseline behavior";
        }
        catch (const std::exception & e)
        {
            m_logger->logError("Exception during empty document save: " + std::string(e.what()));
            FAIL() << "Empty document save threw exception: " << e.what();
        }

        m_logger->logInfo("=== SaveEmptyDocument test completed ===");
    }

    /**
     * @brief Test saving to an invalid path
     *
     * This test attempts to save to an invalid path to verify error handling.
     */
    TEST_F(Writer3mfSaveAsTest, SaveToInvalidPath_InvalidDirectory_ShouldHandleErrorGracefully)
    {
        m_logger->logInfo("=== Starting SaveToInvalidPath test ===");

        // Try to save to a non-existent directory without creating it first
        auto invalidPath =
          std::filesystem::path("/nonexistent/directory/that/should/not/exist/test.3mf");
        m_logger->logInfo("Attempting to save to invalid path: " + invalidPath.string());

        // This should either throw an exception or handle the error gracefully
        try
        {
            m_document->saveAs(invalidPath, true);
            m_logger->logWarning(
              "Save to invalid path did not throw exception - checking if file was created");

            // If no exception was thrown, the file should not exist
            EXPECT_FALSE(std::filesystem::exists(invalidPath))
              << "File should not be created when saving to invalid path";
        }
        catch (const std::exception & e)
        {
            m_logger->logInfo("Expected exception caught for invalid path: " +
                              std::string(e.what()));
            // This is expected behavior
            SUCCEED();
        }

        m_logger->logInfo("=== SaveToInvalidPath test completed ===");
    }

    /**
     * @brief Test multiple consecutive save operations
     *
     * This test performs multiple save operations to identify if the issue
     * is related to document state after saving.
     */
    TEST_F(Writer3mfSaveAsTest, MultipleSaves_SameDocument_ShouldBeConsistent)
    {
        m_logger->logInfo("=== Starting MultipleSaves test ===");

        // Create a function for testing
        auto functionId = createGyroidFunction();
        ASSERT_NE(functionId, 0) << "Failed to create function for multiple saves test";

        // Perform multiple saves
        for (int i = 1; i <= 3; ++i)
        {
            auto testFile = getTestFilePath("multiple_saves_test_" + std::to_string(i) + ".3mf");
            m_logger->logInfo("Save attempt #" + std::to_string(i) + " to: " + testFile.string());

            try
            {
                m_document->saveAs(testFile, true);
                m_logger->logInfo("Save #" + std::to_string(i) + " completed without exception");

                bool isValid = validateSavedFile(testFile);
                EXPECT_TRUE(isValid) << "Save #" << i << " should produce valid file";
            }
            catch (const std::exception & e)
            {
                m_logger->logError("Exception during save #" + std::to_string(i) + ": " +
                                   std::string(e.what()));
                FAIL() << "Save #" << i << " threw exception: " << e.what();
            }
        }

        m_logger->logInfo("=== MultipleSaves test completed ===");
    }
}
