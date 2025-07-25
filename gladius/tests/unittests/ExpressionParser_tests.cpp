#include "ExpressionParser.h"
#include <gtest/gtest.h>

namespace gladius::tests
{
    class ExpressionParserTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            m_parser = std::make_unique<ExpressionParser>();
        }

        std::unique_ptr<ExpressionParser> m_parser;
    };

    TEST_F(ExpressionParserTest, ParseSimpleExpression_ValidInput_ReturnsTrue)
    {
        // Arrange
        std::string const expression = "x + y";

        // Act
        bool const result = m_parser->parseExpression(expression);

        // Assert
        EXPECT_TRUE(result);
        EXPECT_TRUE(m_parser->hasValidExpression());
        EXPECT_TRUE(m_parser->getLastError().empty());
    }

    TEST_F(ExpressionParserTest, ParseInvalidExpression_InvalidSyntax_ReturnsFalse)
    {
        // Arrange
        std::string const expression = "x + )"; // Invalid syntax - unmatched parenthesis

        // Act
        bool const result = m_parser->parseExpression(expression);

        // Assert
        EXPECT_FALSE(result);
        EXPECT_FALSE(m_parser->hasValidExpression());
        EXPECT_FALSE(m_parser->getLastError().empty());
    }

    TEST_F(ExpressionParserTest, GetVariables_SimpleExpression_ReturnsCorrectVariables)
    {
        // Arrange
        std::string const expression = "x + y * z";
        m_parser->parseExpression(expression);

        // Act
        std::vector<std::string> const variables = m_parser->getVariables();

        // Assert
        EXPECT_EQ(variables.size(), 3);
        EXPECT_TRUE(std::find(variables.begin(), variables.end(), "x") != variables.end());
        EXPECT_TRUE(std::find(variables.begin(), variables.end(), "y") != variables.end());
        EXPECT_TRUE(std::find(variables.begin(), variables.end(), "z") != variables.end());
    }

    TEST_F(ExpressionParserTest, GetVariables_NoVariables_ReturnsEmptyVector)
    {
        // Arrange
        std::string const expression = "5 + 3";
        m_parser->parseExpression(expression);

        // Act
        std::vector<std::string> const variables = m_parser->getVariables();

        // Assert
        EXPECT_TRUE(variables.empty());
    }

    TEST_F(ExpressionParserTest, GetExpressionString_ValidExpression_ReturnsString)
    {
        // Arrange
        std::string const expression = "x + y";
        m_parser->parseExpression(expression);

        // Act
        std::string const result = m_parser->getExpressionString();

        // Assert
        EXPECT_FALSE(result.empty());
        // Note: muParser might reformat the expression, so we just check it's not empty
    }

    TEST_F(ExpressionParserTest, GetExpressionString_NoValidExpression_ReturnsEmptyString)
    {
        // Arrange
        // No expression parsed

        // Act
        std::string const result = m_parser->getExpressionString();

        // Assert
        EXPECT_TRUE(result.empty());
    }

    TEST_F(ExpressionParserTest, Evaluate_WithVariables_ReturnsCorrectResult)
    {
        // Arrange
        std::string const expression = "x + y * 2";
        m_parser->parseExpression(expression);
        std::map<std::string, double> variables = {{"x", 3.0}, {"y", 4.0}};

        // Act
        double const result = m_parser->evaluate(variables);

        // Assert
        EXPECT_DOUBLE_EQ(result, 11.0); // 3 + 4 * 2 = 11
    }

    // Vector Component Access Tests
    TEST_F(ExpressionParserTest, ParseExpression_VectorComponentAccess_ValidatesCorrectly)
    {
        // Test that vector component access expressions are properly preprocessed and validated
        EXPECT_TRUE(m_parser->parseExpression("pos.x"));
        EXPECT_TRUE(m_parser->hasValidExpression());

        EXPECT_TRUE(m_parser->parseExpression("pos.y + vel.z"));
        EXPECT_TRUE(m_parser->hasValidExpression());

        EXPECT_TRUE(m_parser->parseExpression("sqrt(normal.x * normal.x + normal.y * normal.y)"));
        EXPECT_TRUE(m_parser->hasValidExpression());
    }

    TEST_F(ExpressionParserTest, ParseExpression_InvalidVectorComponent_FailsValidation)
    {
        // Test that invalid vector components are rejected
        EXPECT_FALSE(m_parser->parseExpression("pos.w")); // Invalid component
        EXPECT_FALSE(m_parser->hasValidExpression());

        EXPECT_FALSE(m_parser->parseExpression("pos.xy")); // Invalid component
        EXPECT_FALSE(m_parser->hasValidExpression());
    }

    TEST_F(ExpressionParserTest, GetVariables_VectorComponentAccess_ReturnsOriginalSyntax)
    {
        // Test that getVariables returns the original dot notation
        m_parser->parseExpression("pos.x + vel.y");
        auto variables = m_parser->getVariables();

        // Should contain the original component access syntax
        EXPECT_NE(std::find(variables.begin(), variables.end(), "pos.x"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "vel.y"), variables.end());
    }

    TEST_F(ExpressionParserTest, GetVariables_MixedVariables_ReturnsCorrectList)
    {
        // Test mixed scalar and vector component variables
        m_parser->parseExpression("scale * pos.x + offset");
        auto variables = m_parser->getVariables();

        // Should contain both scalar and component access variables
        EXPECT_NE(std::find(variables.begin(), variables.end(), "scale"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "pos.x"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "offset"), variables.end());
    }

    TEST_F(ExpressionParserTest, ParseExpression_ComplexVectorExpression_ValidatesCorrectly)
    {
        // Test complex expression with multiple vector components
        std::string expression = "sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z) - radius";
        EXPECT_TRUE(m_parser->parseExpression(expression));
        EXPECT_TRUE(m_parser->hasValidExpression());

        auto variables = m_parser->getVariables();
        EXPECT_NE(std::find(variables.begin(), variables.end(), "pos.x"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "pos.y"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "pos.z"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "radius"), variables.end());
    }

    TEST_F(ExpressionParserTest, ParseExpression_VectorComponentsInFunctions_ValidatesCorrectly)
    {
        // Test vector components used as function arguments
        EXPECT_TRUE(m_parser->parseExpression("sin(angle.x) + cos(angle.y)"));
        EXPECT_TRUE(m_parser->hasValidExpression());

        EXPECT_TRUE(m_parser->parseExpression("sqrt(base.x * base.x + base.y * base.y)"));
        EXPECT_TRUE(m_parser->hasValidExpression());

        EXPECT_TRUE(m_parser->parseExpression("abs(a.x) + exp(a.y)"));
        EXPECT_TRUE(m_parser->hasValidExpression());
    }

    TEST_F(ExpressionParserTest, ParseExpression_NestedVectorExpressions_ValidatesCorrectly)
    {
        // Test nested expressions with vector components
        std::string expression = "(a.x + b.x) * (a.y - b.y) / (a.z * b.z)";
        EXPECT_TRUE(m_parser->parseExpression(expression));
        EXPECT_TRUE(m_parser->hasValidExpression());

        auto variables = m_parser->getVariables();
        EXPECT_NE(std::find(variables.begin(), variables.end(), "a.x"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "b.x"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "a.y"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "b.y"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "a.z"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "b.z"), variables.end());
    }

    TEST_F(ExpressionParserTest,
           ParseExpression_VariableNotPartOfComponentAccess_IncludedInVariables)
    {
        // Test that standalone variables are included even when component access is present
        m_parser->parseExpression("scale + pos.x");
        auto variables = m_parser->getVariables();

        EXPECT_NE(std::find(variables.begin(), variables.end(), "scale"), variables.end());
        EXPECT_NE(std::find(variables.begin(), variables.end(), "pos.x"), variables.end());

        // "pos" alone should not be in the variables list since it's part of "pos.x"
        EXPECT_EQ(std::find(variables.begin(), variables.end(), "pos"), variables.end());
    }

} // namespace gladius::tests
