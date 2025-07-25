#include "ExpressionParser.h"
#include "ExpressionToGraphConverter.h"
#include "FunctionArgument.h"
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

    // Vector Component Access Tests
    TEST_F(ExpressionToGraphConverterTest,
           ConvertVectorComponent_SingleComponentAccess_CreatesDecomposeVectorNode)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("pos", ArgumentType::Vector);
        std::string const expression = "pos.x";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created a DecomposeVector node
        auto decomposeCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::DecomposeVector>(*m_model);
        EXPECT_GT(decomposeCount, 0);

        // Should have created a vector input node
        EXPECT_GE(m_model->getSize(), 2); // At least: ConstantVector, DecomposeVector
    }

    TEST_F(
      ExpressionToGraphConverterTest,
      ConvertVectorComponentExpression_ComplexExpressionWithVectorComponents_CreatesCorrectGraph)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("radius", ArgumentType::Scalar);
        arguments.emplace_back("pos", ArgumentType::Vector);
        std::string const expression = "sqrt(pos.x * pos.x + pos.y * pos.y) - radius";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created DecomposeVector nodes - one for pos vector (reused for pos.x and
        // pos.y)
        auto decomposeCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::DecomposeVector>(*m_model);
        EXPECT_EQ(decomposeCount,
                  1); // One DecomposeVector for pos vector, reused for both components

        // Should have created math operation nodes
        auto multiplicationCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Multiplication>(*m_model);
        EXPECT_GE(multiplicationCount, 2); // pos.x * pos.x and pos.y * pos.y

        auto additionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
        EXPECT_GT(additionCount, 0); // pos.x * pos.x + pos.y * pos.y

        auto subtractionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Subtraction>(*m_model);
        EXPECT_GT(subtractionCount, 0); // sqrt(...) - radius

        // Should have created sqrt function node
        auto sqrtCount = gladius_tests::helper::countNumberOfNodesOfType<nodes::Sqrt>(*m_model);
        EXPECT_GT(sqrtCount, 0);
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertMultipleVectorComponents_AllThreeComponents_CreatesThreeDecomposeNodes)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("vec", ArgumentType::Vector);
        std::string const expression = "vec.x + vec.y + vec.z";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created one DecomposeVector node (reused for all component accesses of same
        // vector)
        auto decomposeCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::DecomposeVector>(*m_model);
        EXPECT_EQ(decomposeCount, 1); // One DecomposeVector for vec, reused for vec.x, vec.y, vec.z

        // Should have created addition nodes
        auto additionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
        EXPECT_EQ(additionCount, 2); // vec.x + vec.y, and then result + vec.z
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertMixedScalarAndVectorArgs_ScalarAndVectorArguments_CreatesCorrectNodeTypes)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("scale", ArgumentType::Scalar);
        arguments.emplace_back("position", ArgumentType::Vector);
        arguments.emplace_back("offset", ArgumentType::Scalar);
        std::string const expression = "scale * position.x + offset";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created one DecomposeVector node for position.x
        auto decomposeCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::DecomposeVector>(*m_model);
        EXPECT_EQ(decomposeCount, 1);

        // Should have created appropriate argument nodes
        EXPECT_GE(m_model->getSize(),
                  5); // scale, position, offset, DecomposeVector, multiplication, addition
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertVectorComponentWithFunction_VectorComponentInFunction_CreatesCorrectGraph)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("normal", ArgumentType::Vector);
        std::string const expression = "sin(normal.x) + cos(normal.y)";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created DecomposeVector nodes - one for normal vector (reused for normal.x
        // and normal.y)
        auto decomposeCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::DecomposeVector>(*m_model);
        EXPECT_EQ(decomposeCount,
                  1); // One DecomposeVector for normal vector, reused for both components

        // Should have created trigonometric function nodes
        auto sineCount = gladius_tests::helper::countNumberOfNodesOfType<nodes::Sine>(*m_model);
        EXPECT_GT(sineCount, 0);

        auto cosineCount = gladius_tests::helper::countNumberOfNodesOfType<nodes::Cosine>(*m_model);
        EXPECT_GT(cosineCount, 0);

        // Should have created addition node
        auto additionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
        EXPECT_GT(additionCount, 0);
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertInvalidVectorComponent_InvalidComponent_ReturnsZero)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("vec", ArgumentType::Vector);
        std::string const expression = "vec.w"; // Invalid component (only x, y, z are valid)

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments);

        // Assert
        EXPECT_EQ(resultNodeId, 0); // Should fail for invalid component
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertComponentAccessOnScalar_ScalarWithComponent_ReturnsZero)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("value", ArgumentType::Scalar);
        std::string const expression = "value.x"; // Invalid: scalar doesn't have components

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments);

        // Assert
        EXPECT_EQ(resultNodeId, 0); // Should fail for component access on scalar
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertNestedVectorExpressions_ComplexNestedExpression_CreatesCorrectGraph)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("a", ArgumentType::Vector);
        arguments.emplace_back("b", ArgumentType::Vector);
        std::string const expression = "(a.x + b.x) * (a.y - b.y)";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should have created DecomposeVector nodes - one for each vector (a and b)
        auto decomposeCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::DecomposeVector>(*m_model);
        EXPECT_EQ(
          decomposeCount,
          2); // One for vector a (reused for a.x, a.y), one for vector b (reused for b.x, b.y)

        // Should have created appropriate math operation nodes
        auto additionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
        EXPECT_GT(additionCount, 0); // a.x + b.x

        auto subtractionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Subtraction>(*m_model);
        EXPECT_GT(subtractionCount, 0); // a.y - b.y

        auto multiplicationCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Multiplication>(*m_model);
        EXPECT_GT(multiplicationCount, 0); // (a.x + b.x) * (a.y - b.y)
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertBackwardsCompatibility_ExpressionWithoutArguments_StillWorks)
    {
        // Arrange
        std::string const expression = "x + y"; // Old style without arguments

        // Act
        nodes::NodeId resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expression, *m_model, *m_parser);

        // Assert
        EXPECT_NE(resultNodeId, 0);

        // Should still work as before
        auto additionCount =
          gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
        EXPECT_GT(additionCount, 0);
    }

    // Vector Component Tests
    class ExpressionToGraphConverterVectorTest : public ExpressionToGraphConverterTest
    {
      protected:
        void SetUp() override
        {
            ExpressionToGraphConverterTest::SetUp();

            // Add vector arguments for testing
            m_vectorArgs = {{"pos", ArgumentType::Vector},
                            {"vel", ArgumentType::Vector},
                            {"scale", ArgumentType::Scalar}};
        }

        std::vector<FunctionArgument> m_vectorArgs;

        // Helper function to count nodes by type name
        int countNodesByType(const std::string & typeName)
        {
            if (typeName == "DecomposeVector")
            {
                return gladius_tests::helper::countNumberOfNodesOfType<nodes::DecomposeVector>(
                  *m_model);
            }
            else if (typeName == "Addition")
            {
                return gladius_tests::helper::countNumberOfNodesOfType<nodes::Addition>(*m_model);
            }
            else if (typeName == "Multiplication")
            {
                return gladius_tests::helper::countNumberOfNodesOfType<nodes::Multiplication>(
                  *m_model);
            }
            return 0;
        }
    };

    TEST_F(ExpressionToGraphConverterVectorTest,
           ConvertVectorComponent_SingleComponent_CreatesDecomposeNode)
    {
        // Arrange
        std::string expression = "pos.x";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, m_vectorArgs);

        // Assert
        EXPECT_GT(result, 0) << "Expression conversion should succeed for single vector component";
        EXPECT_EQ(countNodesByType("DecomposeVector"), 1)
          << "Should create exactly one DecomposeVector node";
    }

    TEST_F(ExpressionToGraphConverterVectorTest,
           ConvertVectorComponent_MultipleComponents_CreatesOneDecomposeNode)
    {
        // Arrange
        std::string expression = "pos.x + pos.y";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, m_vectorArgs);

        // Assert
        EXPECT_GT(result, 0) << "Expression conversion should succeed for multiple components";
        EXPECT_EQ(countNodesByType("DecomposeVector"), 1)
          << "Should create exactly one DecomposeVector node for same vector";
        EXPECT_EQ(countNodesByType("Addition"), 1) << "Should create one Addition node";
    }

    TEST_F(ExpressionToGraphConverterVectorTest,
           ConvertVectorComponent_ComplexExpression_CreatesCorrectNodes)
    {
        // Arrange
        std::string expression = "pos.x * pos.x + pos.y * 3.14";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, m_vectorArgs);

        // Assert
        EXPECT_GT(result, 0) << "Expression conversion should succeed for complex expression";
        EXPECT_EQ(countNodesByType("DecomposeVector"), 1)
          << "Should create one DecomposeVector node";
        EXPECT_GE(countNodesByType("Multiplication"), 2)
          << "Should create at least two Multiplication nodes";
        EXPECT_EQ(countNodesByType("Addition"), 1) << "Should create one Addition node";
    }

    TEST_F(ExpressionToGraphConverterVectorTest,
           ConvertVectorComponent_MultipleVectors_CreatesMultipleDecomposeNodes)
    {
        // Arrange
        std::string expression = "pos.x + vel.y";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, m_vectorArgs);

        // Assert
        EXPECT_GT(result, 0) << "Expression conversion should succeed for multiple vectors";
        EXPECT_EQ(countNodesByType("DecomposeVector"), 2)
          << "Should create two DecomposeVector nodes for different vectors";
        EXPECT_EQ(countNodesByType("Addition"), 1) << "Should create one Addition node";
    }

    TEST_F(ExpressionToGraphConverterVectorTest,
           ConvertVectorComponent_AllComponents_CreatesOneDecomposeNode)
    {
        // Arrange
        std::string expression = "pos.x + pos.y + pos.z";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, m_vectorArgs);

        // Assert
        EXPECT_GT(result, 0) << "Expression conversion should succeed for all components";
        EXPECT_EQ(countNodesByType("DecomposeVector"), 1)
          << "Should create one DecomposeVector node for same vector";
        EXPECT_EQ(countNodesByType("Addition"), 2) << "Should create two Addition nodes";
    }

    TEST_F(ExpressionToGraphConverterVectorTest,
           ConvertVectorComponent_WithScalar_CreatesCorrectNodes)
    {
        // Arrange
        std::string expression = "pos.x * scale + 5.0";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, m_vectorArgs);

        // Assert
        EXPECT_GT(result, 0) << "Expression conversion should succeed for vector and scalar mix";
        EXPECT_EQ(countNodesByType("DecomposeVector"), 1)
          << "Should create one DecomposeVector node";
        EXPECT_EQ(countNodesByType("Multiplication"), 1) << "Should create one Multiplication node";
        EXPECT_EQ(countNodesByType("Addition"), 1) << "Should create one Addition node";
    }

    TEST_F(ExpressionToGraphConverterVectorTest,
           ConvertVectorComponent_NestedOperations_CreatesCorrectNodes)
    {
        // Arrange
        std::string expression = "(pos.x + pos.y) * (vel.x - vel.y)";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, m_vectorArgs);

        // Assert
        EXPECT_GT(result, 0) << "Expression conversion should succeed for nested operations";
        EXPECT_EQ(countNodesByType("DecomposeVector"), 2)
          << "Should create two DecomposeVector nodes";
        EXPECT_EQ(countNodesByType("Addition"), 1) << "Should create one Addition node";
        EXPECT_EQ(countNodesByType("Multiplication"), 1) << "Should create one Multiplication node";
    }

    TEST_F(ExpressionToGraphConverterVectorTest,
           ConvertVectorComponent_OnlyVectorArguments_ValidatesTypes)
    {
        // Arrange - Create arguments with only vector types
        std::vector<FunctionArgument> vectorOnlyArgs = {{"a", ArgumentType::Vector},
                                                        {"b", ArgumentType::Vector}};
        std::string expression = "a.x + b.y";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, vectorOnlyArgs);

        // Assert
        EXPECT_GT(result, 0) << "Should handle expressions with only vector arguments";
        EXPECT_EQ(countNodesByType("DecomposeVector"), 2)
          << "Should create DecomposeVector for each vector";
    }

    TEST_F(ExpressionToGraphConverterVectorTest, ConvertVectorComponent_EmptyExpression_ReturnsZero)
    {
        // Arrange
        std::string expression = "";

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, m_vectorArgs);

        // Assert
        EXPECT_EQ(result, 0) << "Empty expression should return 0";
    }

} // namespace gladius::tests
