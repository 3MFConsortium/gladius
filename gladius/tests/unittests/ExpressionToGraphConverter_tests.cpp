#include "ExpressionParser.h"
#include "ExpressionToGraphConverter.h"
#include "nodes/DerivedNodes.h"
#include "nodes/Model.h"
#include "nodes/NodeBase.h"
#include "nodes/nodesfwd.h"
#include "testhelper.h"
#include <gtest/gtest.h>

namespace gladius::tests
{
    class ExpressionToGraphConverterTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            m_model = std::make_unique<nodes::Model>();
            m_parser = std::make_unique<ExpressionParser>();
        }

        std::unique_ptr<nodes::Model> m_model;
        std::unique_ptr<ExpressionParser> m_parser;
    };

    TEST_F(ExpressionToGraphConverterTest,
           ConvertSimpleAddition_ValidExpression_CreatesCorrectGraph)
    {
        // Arrange
        std::string const expression = "x + y";

        // Act
        nodes::NodeId resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expression, *m_model, *m_parser);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Check that nodes were created
        EXPECT_GT(m_model->getSize(), 0);

        // Find the addition node
        auto additionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
        EXPECT_GT(additionCount, 0);
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertComplexExpression_ValidExpression_CreatesMultipleNodes)
    {
        // Arrange
        std::string const expression = "x + y * z";

        // Act
        nodes::NodeId resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expression, *m_model, *m_parser);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created multiple nodes (at least addition and multiplication)
        EXPECT_GE(m_model->getSize(), 3); // At least: x, y, z, multiplication, addition

        // Check for both addition and multiplication nodes
        auto additionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
        auto multiplicationCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Multiplication>(*m_model);

        EXPECT_GT(additionCount, 0);
        EXPECT_GT(multiplicationCount, 0);
    }

    TEST_F(ExpressionToGraphConverterTest, ConvertInvalidExpression_InvalidSyntax_ReturnsZero)
    {
        // Arrange
        std::string const expression = "x + )"; // Invalid syntax

        // Act
        nodes::NodeId resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expression, *m_model, *m_parser);

        // Assert
        EXPECT_EQ(resultNodeId, 0);
    }

    TEST_F(ExpressionToGraphConverterTest, ConvertConstantExpression_NumberOnly_CreatesConstantNode)
    {
        // Arrange
        std::string const expression = "42";

        // Act
        nodes::NodeId resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expression, *m_model, *m_parser);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created at least one node
        EXPECT_GE(m_model->getSize(), 1);
    }

    TEST_F(ExpressionToGraphConverterTest, ConvertSingleVariable_VariableOnly_CreatesVariableNode)
    {
        // Arrange
        std::string const expression = "x";

        // Act
        nodes::NodeId resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expression, *m_model, *m_parser);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created at least one node
        EXPECT_GE(m_model->getSize(), 1);
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertExpressionWithParentheses_ValidExpression_HandlesCorrectly)
    {
        // Arrange
        std::string const expression = "(x + y) * z";

        // Act
        nodes::NodeId resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expression, *m_model, *m_parser);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created nodes for addition and multiplication
        auto additionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
        auto multiplicationCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Multiplication>(*m_model);

        EXPECT_GT(additionCount, 0);
        EXPECT_GT(multiplicationCount, 0);
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertAllBasicOperations_AllOperators_CreatesCorrectNodes)
    {
        // Arrange
        std::vector<std::pair<std::string, std::function<int(nodes::Model &)>>> testCases = {
          {"x + y",
           [](nodes::Model & model)
           { return gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(model); }},
          {"x - y",
           [](nodes::Model & model)
           { return gladius_tests::helper::countNumberOfNodesOfType<nodes::Subtraction>(model); }},
          {"x * y",
           [](nodes::Model & model) {
               return gladius_tests::helper::countNumberOfNodesOfType<nodes::Multiplication>(model);
           }},
          {"x / y", [](nodes::Model & model) {
               return gladius_tests::helper::countNumberOfNodesOfType<nodes::Division>(model);
           }}};

        for (auto const & [expression, countFunction] : testCases)
        {
            // Create a fresh model for each test
            auto model = std::make_unique<nodes::Model>();

            // Act
            nodes::NodeId resultNodeId =
              ExpressionToGraphConverter::convertExpressionToGraph(expression, *model, *m_parser);

            // Assert
            EXPECT_NE(resultNodeId, 0) << "Failed for expression: " << expression;
            EXPECT_GT(countFunction(*model), 0)
              << "Did not find expected node for expression: " << expression;
        }
    }

} // namespace gladius::tests
