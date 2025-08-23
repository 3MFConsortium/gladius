#include "Application.h"
#include "mcp/ApplicationMCPAdapter.h"
#include <gtest/gtest.h>

namespace gladius::tests
{
    class ApplicationMCPAdapterRollbackTest : public ::testing::Test
    {
      protected:
        std::unique_ptr<Application> m_app;
        std::unique_ptr<ApplicationMCPAdapter> m_adapter;

        void SetUp() override
        {
            // Initialize application in headless mode so we can operate without full UI
            m_app = std::make_unique<Application>(true /*headless*/);
            m_adapter = std::make_unique<ApplicationMCPAdapter>(m_app.get());

            // Ensure we have a fresh document
            ASSERT_TRUE(m_adapter->createNewDocument());
            ASSERT_TRUE(m_adapter->hasActiveDocument());
        }
    };

    TEST_F(ApplicationMCPAdapterRollbackTest,
           CreateFunctionFromExpression_InvalidComponent_RollsBackWithoutResiduals)
    {
        // Arrange
        auto document = m_app->getCurrentDocument();
        ASSERT_TRUE(static_cast<bool>(document));

        auto assembly = document->getAssembly();
        ASSERT_TRUE(static_cast<bool>(assembly));
        auto const initialFunctionCount = assembly->getFunctions().size();

        // Use an expression that passes parsing but fails conversion (invalid vector component .w)
        std::vector<FunctionArgument> args = {FunctionArgument{"pos", ArgumentType::Vector}};

        // Act
        auto const result = m_adapter->createFunctionFromExpression(
          "invalid_component_test", "sin(pos).w", "float", args, "result");

        // Assert
        EXPECT_FALSE(result.first) << "Creation should fail due to invalid component";
        EXPECT_EQ(result.second, 0u) << "Resource ID should be 0 on failure";

        // Verify no new functions were left behind (rollback worked)
        auto const afterFunctionCount = assembly->getFunctions().size();
        EXPECT_EQ(afterFunctionCount, initialFunctionCount)
          << "Function count should remain unchanged after failed creation";
    }

    TEST_F(ApplicationMCPAdapterRollbackTest,
           CreateFunctionFromExpression_ValidExpression_CreatesFunctionSuccessfully)
    {
        // Arrange
        auto document = m_app->getCurrentDocument();
        ASSERT_TRUE(static_cast<bool>(document));
        auto assembly = document->getAssembly();
        ASSERT_TRUE(static_cast<bool>(assembly));

        auto const initialFunctionCount = assembly->getFunctions().size();
        std::vector<FunctionArgument> args = {FunctionArgument{"pos", ArgumentType::Vector}};

        // A valid scalar expression using vector components
        std::string const expr = "pos.x + pos.y";

        // Act
        auto const created =
          m_adapter->createFunctionFromExpression("valid_function", expr, "float", args, "shape");

        // Assert success and valid resource id
        EXPECT_TRUE(created.first) << "Creation should succeed for valid expression";
        EXPECT_GT(created.second, 0u) << "Resource ID should be > 0 on success";

        // Confirm function count increased by 1
        auto const afterFunctionCount = assembly->getFunctions().size();
        EXPECT_EQ(afterFunctionCount, initialFunctionCount + 1)
          << "Function count should increase by 1 after successful creation";
    }
} // namespace gladius::tests
