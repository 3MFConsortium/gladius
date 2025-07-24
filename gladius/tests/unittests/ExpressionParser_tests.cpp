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

} // namespace gladius::tests
