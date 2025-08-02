#include "mcp/ApplicationMCPAdapter.h"
#include "mcp/MCPApplicationInterface.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>

namespace gladius::tests
{
    // Note: Since Application is a complex class, we'll test the adapter's behavior
    // with a minimal setup and focus on the MCP interface compliance
    class ApplicationMCPAdapterTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            // We'll test with a null application pointer to verify error handling
            m_adapter = std::make_unique<ApplicationMCPAdapter>(nullptr);
        }

        std::unique_ptr<ApplicationMCPAdapter> m_adapter;
    };

    // Test basic info methods with null application
    TEST_F(ApplicationMCPAdapterTest, GetVersion_NullApplication_ReturnsUnknown)
    {
        // Act
        std::string version = m_adapter->getVersion();

        // Assert
        EXPECT_EQ(version, "Unknown");
    }

    TEST_F(ApplicationMCPAdapterTest, IsRunning_NullApplication_ReturnsFalse)
    {
        // Act
        bool isRunning = m_adapter->isRunning();

        // Assert
        EXPECT_FALSE(isRunning);
    }

    TEST_F(ApplicationMCPAdapterTest, GetApplicationName_Always_ReturnsGladius)
    {
        // Act
        std::string name = m_adapter->getApplicationName();

        // Assert
        EXPECT_EQ(name, "Gladius");
    }

    TEST_F(ApplicationMCPAdapterTest, GetStatus_NullApplication_ReturnsNotRunning)
    {
        // Act
        std::string status = m_adapter->getStatus();

        // Assert
        EXPECT_EQ(status, "not_running");
    }

    TEST_F(ApplicationMCPAdapterTest, HasActiveDocument_NullApplication_ReturnsFalse)
    {
        // Act
        bool hasDoc = m_adapter->hasActiveDocument();

        // Assert
        EXPECT_FALSE(hasDoc);
    }

    TEST_F(ApplicationMCPAdapterTest, GetActiveDocumentPath_NullApplication_ReturnsEmpty)
    {
        // Act
        std::string path = m_adapter->getActiveDocumentPath();

        // Assert
        EXPECT_TRUE(path.empty());
    }

    // Test document operations with null application
    TEST_F(ApplicationMCPAdapterTest, CreateNewDocument_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->createNewDocument();

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest, OpenDocument_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->openDocument("/test/path.3mf");

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest, SaveDocument_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->saveDocument();

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest, SaveDocumentAs_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->saveDocumentAs("/test/output.3mf");

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest, ExportDocument_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->exportDocument("/test/output.stl", "stl");

        // Assert
        EXPECT_FALSE(result);
    }

    // Test parameter operations
    TEST_F(ApplicationMCPAdapterTest, SetFloatParameter_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->setFloatParameter(1, "test_node", "test_param", 42.5f);

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest, GetFloatParameter_NullApplication_ReturnsZero)
    {
        // Act
        float result = m_adapter->getFloatParameter(1, "test_node", "test_param");

        // Assert
        EXPECT_EQ(result, 0.0f);
    }

    TEST_F(ApplicationMCPAdapterTest, SetStringParameter_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->setStringParameter(1, "test_node", "test_param", "test_value");

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest, GetStringParameter_NullApplication_ReturnsEmpty)
    {
        // Act
        std::string result = m_adapter->getStringParameter(1, "test_node", "test_param");

        // Assert
        EXPECT_TRUE(result.empty());
    }

    // Test expression function creation (placeholder implementation)
    TEST_F(ApplicationMCPAdapterTest, CreateFunctionFromExpression_NullApplication_ReturnsFalse)
    {
        // Act
        bool result =
          m_adapter->createFunctionFromExpression("test_function", "sin(x) + cos(y)", "float");

        // Assert
        EXPECT_FALSE(result); // Current implementation returns false until fully implemented
    }

    // Test TPMS expressions
    TEST_F(ApplicationMCPAdapterTest,
           CreateFunctionFromExpression_GyroidExpression_HandlesGracefully)
    {
        // Act
        bool result = m_adapter->createFunctionFromExpression(
          "gyroid",
          "sin(x*2*pi/10)*cos(y*2*pi/10) + sin(y*2*pi/10)*cos(z*2*pi/10) + "
          "sin(z*2*pi/10)*cos(x*2*pi/10) - 0.2",
          "float");

        // Assert
        EXPECT_FALSE(
          result); // Returns false until implementation is complete, but should not crash
    }

    // Test expression validation scenarios
    TEST_F(ApplicationMCPAdapterTest, CreateFunctionFromExpression_EmptyName_HandlesGracefully)
    {
        // Act
        bool result = m_adapter->createFunctionFromExpression("", "sin(x)", "float");

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest,
           CreateFunctionFromExpression_EmptyExpression_HandlesGracefully)
    {
        // Act
        bool result = m_adapter->createFunctionFromExpression("test_function", "", "float");

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest,
           CreateFunctionFromExpression_InvalidOutputType_HandlesGracefully)
    {
        // Act
        bool result =
          m_adapter->createFunctionFromExpression("test_function", "sin(x)", "invalid_type");

        // Assert
        EXPECT_FALSE(result);
    }

    // Test mathematical expression patterns
    class ExpressionPatternTest
        : public ApplicationMCPAdapterTest,
          public ::testing::WithParamInterface<std::tuple<std::string, std::string>>
    {
    };

    TEST_P(ExpressionPatternTest, CreateFunctionFromExpression_CommonPatterns_HandlesGracefully)
    {
        // Arrange
        auto [name, expression] = GetParam();

        // Act
        bool result = m_adapter->createFunctionFromExpression(name, expression, "float");

        // Assert
        EXPECT_FALSE(result); // All should return false until implementation is complete
    }

    INSTANTIATE_TEST_SUITE_P(
      CommonMathematicalExpressions,
      ExpressionPatternTest,
      ::testing::Values(
        std::make_tuple("sphere", "sqrt(x*x + y*y + z*z) - 5"),
        std::make_tuple("box", "max(abs(x) - 5, max(abs(y) - 3, abs(z) - 2))"),
        std::make_tuple("gyroid",
                        "sin(x*2*pi/10)*cos(y*2*pi/10) + sin(y*2*pi/10)*cos(z*2*pi/10) + "
                        "sin(z*2*pi/10)*cos(x*2*pi/10) - 0.2"),
        std::make_tuple("schwarz", "cos(x*2*pi/10) + cos(y*2*pi/10) + cos(z*2*pi/10) - 0.5"),
        std::make_tuple("diamond",
                        "sin(x*2*pi/10)*sin(y*2*pi/10)*sin(z*2*pi/10) + "
                        "sin(x*2*pi/10)*cos(y*2*pi/10)*cos(z*2*pi/10) - 0.3"),
        std::make_tuple("torus", "sqrt((sqrt(x*x + y*y) - 5)*(sqrt(x*x + y*y) - 5) + z*z) - 1"),
        std::make_tuple("cylinder", "sqrt(x*x + y*y) - 3"),
        std::make_tuple("plane", "z")));

    // ========================================
    // Save Document Tests
    // ========================================

    TEST_F(ApplicationMCPAdapterTest, SaveDocument_NullApplication_ReturnsFalseWithErrorMessage)
    {
        // Act
        bool result = m_adapter->saveDocument();

        // Assert
        EXPECT_FALSE(result);
        std::string errorMessage = m_adapter->getLastErrorMessage();
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("No application instance available"));
    }

    TEST_F(ApplicationMCPAdapterTest, SaveDocumentAs_NullApplication_ReturnsFalseWithErrorMessage)
    {
        // Act
        bool result = m_adapter->saveDocumentAs("/tmp/test.3mf");

        // Assert
        EXPECT_FALSE(result);
        std::string errorMessage = m_adapter->getLastErrorMessage();
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("No application instance available"));
    }

    TEST_F(ApplicationMCPAdapterTest, SaveDocumentAs_EmptyPath_ReturnsFalseWithErrorMessage)
    {
        // Act
        bool result = m_adapter->saveDocumentAs("");

        // Assert
        EXPECT_FALSE(result);
        std::string errorMessage = m_adapter->getLastErrorMessage();
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("File path cannot be empty"));
    }

    TEST_F(ApplicationMCPAdapterTest, SaveDocumentAs_InvalidExtension_ReturnsFalseWithErrorMessage)
    {
        // Act
        bool result = m_adapter->saveDocumentAs("/tmp/test.txt");

        // Assert
        EXPECT_FALSE(result);
        std::string errorMessage = m_adapter->getLastErrorMessage();
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("File must have .3mf extension"));
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("/tmp/test.txt"));
    }

    TEST_F(ApplicationMCPAdapterTest, SaveDocumentAs_NoExtension_ReturnsFalseWithErrorMessage)
    {
        // Act
        bool result = m_adapter->saveDocumentAs("/tmp/test_file");

        // Assert
        EXPECT_FALSE(result);
        std::string errorMessage = m_adapter->getLastErrorMessage();
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("File must have .3mf extension"));
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("/tmp/test_file"));
    }

    class SaveDocumentAsPathTest
        : public ApplicationMCPAdapterTest,
          public ::testing::WithParamInterface<std::pair<std::string, std::string>>
    {
    };

    TEST_P(SaveDocumentAsPathTest, SaveDocumentAs_VariousInvalidPaths_ReturnsAppropriateErrors)
    {
        // Arrange
        auto [path, expectedErrorSubstring] = GetParam();

        // Act
        bool result = m_adapter->saveDocumentAs(path);

        // Assert
        EXPECT_FALSE(result);
        std::string errorMessage = m_adapter->getLastErrorMessage();
        EXPECT_THAT(errorMessage, ::testing::HasSubstr(expectedErrorSubstring));
    }

    INSTANTIATE_TEST_SUITE_P(
      InvalidPathTests,
      SaveDocumentAsPathTest,
      ::testing::Values(std::make_pair("", "File path cannot be empty"),
                        std::make_pair("no_extension", "File must have .3mf extension"),
                        std::make_pair("wrong.stl", "File must have .3mf extension"),
                        std::make_pair("wrong.obj", "File must have .3mf extension"),
                        std::make_pair("multiple.dots.txt", "File must have .3mf extension"),
                        std::make_pair("/path/file.3MF",
                                       "File must have .3mf extension"), // Case sensitive test
                        std::make_pair("relative/path/file.xml", "File must have .3mf extension"),
                        std::make_pair("/absolute/path.doc", "File must have .3mf extension")));

    // ========================================
    // Error Message Tests
    // ========================================

    TEST_F(ApplicationMCPAdapterTest, GetLastErrorMessage_InitialState_ReturnsEmptyString)
    {
        // Act
        std::string errorMessage = m_adapter->getLastErrorMessage();

        // Assert
        EXPECT_TRUE(errorMessage.empty());
    }

    TEST_F(ApplicationMCPAdapterTest, GetLastErrorMessage_AfterSaveError_ReturnsDetailedMessage)
    {
        // Arrange
        m_adapter->saveDocumentAs("invalid_file");

        // Act
        std::string errorMessage = m_adapter->getLastErrorMessage();

        // Assert
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("File must have .3mf extension"));
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("invalid_file"));
    }

    TEST_F(ApplicationMCPAdapterTest, GetLastErrorMessage_AfterMultipleErrors_ReturnsLatestMessage)
    {
        // Arrange
        m_adapter->saveDocumentAs("");         // First error
        m_adapter->saveDocumentAs("test.txt"); // Second error

        // Act
        std::string errorMessage = m_adapter->getLastErrorMessage();

        // Assert - Should contain the latest error message
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("File must have .3mf extension"));
        EXPECT_THAT(errorMessage, ::testing::HasSubstr("test.txt"));
        EXPECT_THAT(errorMessage,
                    ::testing::Not(::testing::HasSubstr("File path cannot be empty")));
    }
}
