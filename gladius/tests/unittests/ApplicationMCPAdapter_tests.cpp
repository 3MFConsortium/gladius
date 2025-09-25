#include "mcp/ApplicationMCPAdapter.h"
#include "mcp/MCPApplicationInterface.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <nlohmann/json.hpp>

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
        auto result =
          m_adapter->createFunctionFromExpression("test_function", "sin(x) + cos(y)", "float");

        // Assert
        EXPECT_FALSE(result.first); // Current implementation returns false until fully implemented
        EXPECT_EQ(result.second, 0u); // Should return 0 for resource ID on failure
    }

    // Test TPMS expressions
    TEST_F(ApplicationMCPAdapterTest,
           CreateFunctionFromExpression_GyroidExpression_HandlesGracefully)
    {
        // Act
        auto result = m_adapter->createFunctionFromExpression(
          "gyroid",
          "sin(x*2*pi/10)*cos(y*2*pi/10) + sin(y*2*pi/10)*cos(z*2*pi/10) + "
          "sin(z*2*pi/10)*cos(x*2*pi/10) - 0.2",
          "float");

        // Assert
        EXPECT_FALSE(
          result.first); // Returns false until implementation is complete, but should not crash
        EXPECT_EQ(result.second, 0u);
    }

    // Test expression validation scenarios
    TEST_F(ApplicationMCPAdapterTest, CreateFunctionFromExpression_EmptyName_HandlesGracefully)
    {
        // Act
        auto result = m_adapter->createFunctionFromExpression("", "sin(x)", "float");

        // Assert
        EXPECT_FALSE(result.first); // Empty name should be rejected
        EXPECT_EQ(result.second, 0u);
    }

    TEST_F(ApplicationMCPAdapterTest,
           CreateFunctionFromExpression_EmptyExpression_HandlesGracefully)
    {
        // Act
        auto result = m_adapter->createFunctionFromExpression("test_function", "", "float");

        // Assert
        EXPECT_FALSE(result.first);
        EXPECT_EQ(result.second, 0u);
    }

    TEST_F(ApplicationMCPAdapterTest,
           CreateFunctionFromExpression_InvalidOutputType_HandlesGracefully)
    {
        // Act
        auto result =
          m_adapter->createFunctionFromExpression("test_function", "sin(x)", "invalid_type");

        // Assert
        EXPECT_FALSE(result.first);
        EXPECT_EQ(result.second, 0u);
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
        auto result = m_adapter->createFunctionFromExpression(name, expression, "float");

        // Assert
        EXPECT_FALSE(result.first); // All should return false until implementation is complete
        EXPECT_EQ(result.second, 0u);
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

    // Test rendering and thumbnail generation methods
    TEST_F(ApplicationMCPAdapterTest, GenerateThumbnail_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->generateThumbnail("/test/thumbnail.png", 256);

        // Assert
        EXPECT_FALSE(result);
        // Verify error message indicates no active document
        EXPECT_THAT(m_adapter->getLastErrorMessage(),
                    ::testing::HasSubstr("No active document available"));
    }

    TEST_F(ApplicationMCPAdapterTest, RenderToFile_NullApplication_ReturnsFalse)
    {
        // Act
        bool result = m_adapter->renderToFile("/test/render.png", 512, 512, "png", 0.9f);

        // Assert
        EXPECT_FALSE(result);
        // Verify error message indicates no active document
        EXPECT_THAT(m_adapter->getLastErrorMessage(),
                    ::testing::HasSubstr("No active document available"));
    }

    TEST_F(ApplicationMCPAdapterTest, RenderWithCamera_NullApplication_ReturnsFalse)
    {
        // Arrange
        nlohmann::json cameraSettings = {
          {"eye_position", {10, 10, 10}}, {"target_position", {0, 0, 0}}, {"up_vector", {0, 0, 1}}};
        nlohmann::json renderSettings = {{"width", 1024}, {"height", 1024}, {"format", "png"}};

        // Act
        bool result =
          m_adapter->renderWithCamera("/test/camera_render.png", cameraSettings, renderSettings);

        // Assert
        EXPECT_FALSE(result);
        // Verify error message indicates no active document
        EXPECT_THAT(m_adapter->getLastErrorMessage(),
                    ::testing::HasSubstr("No active document available"));
    }

    TEST_F(ApplicationMCPAdapterTest, GetOptimalCameraPosition_NullApplication_ReturnsError)
    {
        // Act
        auto result = m_adapter->getOptimalCameraPosition();

        // Assert
        EXPECT_TRUE(result.contains("error"));
        EXPECT_THAT(result["error"].get<std::string>(),
                    ::testing::HasSubstr("No active document available"));
    }

    // Test error message propagation for rendering methods
    TEST_F(ApplicationMCPAdapterTest, RenderingMethods_NullApplication_ProperErrorMessages)
    {
        // Test that all rendering methods provide clear error messages

        // Test generateThumbnail
        m_adapter->generateThumbnail("/test/thumb.png", 128);
        EXPECT_THAT(m_adapter->getLastErrorMessage(),
                    ::testing::HasSubstr("No active document available"));

        // Test renderToFile
        m_adapter->renderToFile("/test/render.png", 256, 256);
        EXPECT_THAT(m_adapter->getLastErrorMessage(),
                    ::testing::HasSubstr("No active document available"));

        // Test renderWithCamera
        nlohmann::json emptyCam, emptyRender;
        m_adapter->renderWithCamera("/test/cam.png", emptyCam, emptyRender);
        EXPECT_THAT(m_adapter->getLastErrorMessage(),
                    ::testing::HasSubstr("No active document available"));
    }

    // Test parameter validation for rendering methods
    TEST_F(ApplicationMCPAdapterTest, RenderToFile_InvalidFormat_HandlesGracefully)
    {
        // Act
        bool result = m_adapter->renderToFile("/test/render.jpg", 512, 512, "unsupported", 0.9f);

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest, GenerateThumbnail_ZeroSize_HandlesGracefully)
    {
        // Act
        bool result = m_adapter->generateThumbnail("/test/thumbnail.png", 0);

        // Assert
        EXPECT_FALSE(result);
    }

    TEST_F(ApplicationMCPAdapterTest, RenderToFile_ZeroDimensions_HandlesGracefully)
    {
        // Act
        bool result = m_adapter->renderToFile("/test/render.png", 0, 0);

        // Assert
        EXPECT_FALSE(result);
    }

} // namespace gladius::tests

using gladius::tests::ApplicationMCPAdapterTest;
