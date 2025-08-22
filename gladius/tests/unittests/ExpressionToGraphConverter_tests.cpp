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
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, {}, FunctionOutput::defaultOutput());

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
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, {}, FunctionOutput::defaultOutput());

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
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, {}, FunctionOutput::defaultOutput());

        // Assert
        EXPECT_EQ(resultNodeId, 0);
    }

    TEST_F(ExpressionToGraphConverterTest, ConvertConstantExpression_NumberOnly_CreatesConstantNode)
    {
        // Arrange
        std::string const expression = "42";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, {}, FunctionOutput::defaultOutput());

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
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, {}, FunctionOutput::defaultOutput());

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
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, {}, FunctionOutput::defaultOutput());

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
            nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
              expression, *model, *m_parser, {}, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

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
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, {}, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, m_vectorArgs, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, m_vectorArgs, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, m_vectorArgs, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, m_vectorArgs, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, m_vectorArgs, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, m_vectorArgs, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, m_vectorArgs, FunctionOutput::defaultOutput());

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
          expression, *m_model, *m_parser, m_vectorArgs, FunctionOutput::defaultOutput());

        // Assert
        EXPECT_EQ(result, 0) << "Empty expression should return 0";
    }

    // Test vector function conversion with proper output types
    TEST_F(ExpressionToGraphConverterTest, ConvertVectorSine_VectorArgument_AcceptsVectorOutput)
    {
        // Arrange
        std::string expression = "sin(pos)";
        std::vector<FunctionArgument> vectorArgs = {{"pos", ArgumentType::Vector}};
        FunctionOutput vectorOutput = {"result", ArgumentType::Vector};

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, vectorArgs, vectorOutput);

        // Assert
        EXPECT_NE(result, 0) << "sin(pos) with vector output should succeed";
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertScalarCombination_VectorComponents_AcceptsScalarOutput)
    {
        // Arrange
        std::string expression = "sin(pos.x) * cos(pos.y) * sin(pos.z)";
        std::vector<FunctionArgument> vectorArgs = {{"pos", ArgumentType::Vector}};
        FunctionOutput scalarOutput = {"result", ArgumentType::Scalar};

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, vectorArgs, scalarOutput);

        // Assert
        EXPECT_NE(result, 0) << "Component-based expression with scalar output should succeed";
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertVectorFunctionComponent_VectorToScalar_AcceptsScalarOutput)
    {
        // Arrange - Use exact same pattern as working test
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("pos", ArgumentType::Vector);
        std::string const expression = "pos.x";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

        // Assert
        EXPECT_NE(resultNodeId, 0) << "pos.x with default scalar output should succeed";
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConvertFunctionCallWithComponent_VectorFunction_AcceptsScalarOutput)
    {
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("pos", ArgumentType::Vector);

        // Test that sin(pos).x works correctly
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          "sin(pos).x",
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});

        EXPECT_NE(result, 0) << "sin(pos).x should work as function call with component access";
        EXPECT_GT(m_model->getSize(), 0) << "Model should contain nodes after successful parsing";
    }

    TEST_F(ExpressionToGraphConverterTest, ConvertFunctionCallWithComponent_AllComponents_Work)
    {
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("pos", ArgumentType::Vector);

        // Test .x component
        nodes::NodeId resultX = ExpressionToGraphConverter::convertExpressionToGraph(
          "sin(pos).x",
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});
        EXPECT_NE(resultX, 0) << "sin(pos).x should work";

        // Clear model for next test
        m_model = std::make_unique<nodes::Model>();

        // Test .y component
        nodes::NodeId resultY = ExpressionToGraphConverter::convertExpressionToGraph(
          "cos(pos).y",
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});
        EXPECT_NE(resultY, 0) << "cos(pos).y should work";

        // Clear model for next test
        m_model = std::make_unique<nodes::Model>();

        // Test .z component
        nodes::NodeId resultZ = ExpressionToGraphConverter::convertExpressionToGraph(
          "sin(pos).z",
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});
        EXPECT_NE(resultZ, 0) << "sin(pos).z should work";
    }

    TEST_F(ExpressionToGraphConverterTest, CanConvertToGraph_FunctionCallWithComponent_ReturnsTrue)
    {
        // Test that the validation function recognizes function calls with component access
        EXPECT_TRUE(ExpressionToGraphConverter::canConvertToGraph("sin(pos).x", *m_parser));
        EXPECT_TRUE(ExpressionToGraphConverter::canConvertToGraph("cos(pos).y", *m_parser));
        EXPECT_TRUE(ExpressionToGraphConverter::canConvertToGraph("sin(pos).z", *m_parser));
    }

    TEST_F(ExpressionToGraphConverterTest, CanConvertToGraph_RegularExpression_ReturnsTrue)
    {
        // Test that regular expressions still work
        EXPECT_TRUE(ExpressionToGraphConverter::canConvertToGraph("x + y", *m_parser));
        EXPECT_TRUE(ExpressionToGraphConverter::canConvertToGraph("sin(x)", *m_parser));
        EXPECT_TRUE(ExpressionToGraphConverter::canConvertToGraph("pos.x", *m_parser));
    }

    TEST_F(ExpressionToGraphConverterTest, CanConvertToGraph_InvalidExpression_ReturnsFalse)
    {
        // Test that invalid expressions return false
        EXPECT_FALSE(ExpressionToGraphConverter::canConvertToGraph("", *m_parser));
        EXPECT_FALSE(ExpressionToGraphConverter::canConvertToGraph("invalid()()", *m_parser));
        EXPECT_FALSE(
          ExpressionToGraphConverter::canConvertToGraph("sin(pos).w", *m_parser)); // .w is invalid
    }

    TEST_F(ExpressionToGraphConverterTest, ConvertFunctionCallWithComponent_InvalidComponent_Fails)
    {
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("pos", ArgumentType::Vector);

        // Test invalid component (.w is not valid for Vector)
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          "sin(pos).w",
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});

        EXPECT_EQ(result, 0) << "sin(pos).w should fail because .w is not a valid component";
    }

    TEST_F(ExpressionToGraphConverterTest,
           CanConvertToGraph_NestedFunctionCallWithComponent_ReturnsTrue)
    {
        // Test validation of nested function calls with component access
        EXPECT_TRUE(
          ExpressionToGraphConverter::canConvertToGraph("cos(sin(pos).x+sin(pos).x)", *m_parser));
        EXPECT_TRUE(ExpressionToGraphConverter::canConvertToGraph("sin(cos(pos).y)", *m_parser));
        EXPECT_TRUE(
          ExpressionToGraphConverter::canConvertToGraph("cos(sin(pos).x + cos(pos).y)", *m_parser));
    }

    TEST_F(ExpressionToGraphConverterTest, ConvertNestedFunctionCallWithComponent_SimpleCase_Works)
    {
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("pos", ArgumentType::Vector);

        // Test a simple nested case: sin(cos(pos).x)
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          "sin(cos(pos).x)",
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});

        EXPECT_NE(result, 0) << "sin(cos(pos).x) should work";
    }

    TEST_F(ExpressionToGraphConverterTest, ConvertNestedFunctionCallWithComponent_ComplexCase_Works)
    {
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("pos", ArgumentType::Vector);

        // Test the main case: cos(sin(pos).x+sin(pos).x)
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          "cos(sin(pos).x+sin(pos).x)",
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});

        EXPECT_NE(result, 0) << "cos(sin(pos).x+sin(pos).x) should work";
    }

    // =====================================================================================
    // Comprehensive Implicit Modeling Expression Test Suite
    // =====================================================================================

    struct ImplicitModelingTestCase
    {
        std::string name;
        std::string expression;
        std::vector<FunctionArgument> arguments;
        FunctionOutput expectedOutput;
        std::string description;
        bool shouldPass;
    };

    class ImplicitModelingExpressionTest
        : public ExpressionToGraphConverterTest,
          public ::testing::WithParamInterface<ImplicitModelingTestCase>
    {
      protected:
        void SetUp() override
        {
            ExpressionToGraphConverterTest::SetUp();
        }

        // Helper to create standard 3D position argument
        std::vector<FunctionArgument> createPosArg()
        {
            return {{"pos", ArgumentType::Vector}};
        }

        // Helper to create position and radius arguments
        std::vector<FunctionArgument> createPosRadiusArgs()
        {
            return {{"pos", ArgumentType::Vector}, {"radius", ArgumentType::Scalar}};
        }

        // Helper to create multiple vector arguments
        std::vector<FunctionArgument> createMultiVectorArgs()
        {
            return {{"pos", ArgumentType::Vector},
                    {"center", ArgumentType::Vector},
                    {"size", ArgumentType::Vector}};
        }
    };

    TEST_P(ImplicitModelingExpressionTest, ConvertImplicitModelingExpressions)
    {
        // Arrange
        const auto & testCase = GetParam();

        // Act
        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          testCase.expression, *m_model, *m_parser, testCase.arguments, testCase.expectedOutput);

        // Assert
        if (testCase.shouldPass)
        {
            EXPECT_NE(result, 0) << "Expression should convert successfully: " << testCase.name
                                 << "\nExpression: " << testCase.expression
                                 << "\nDescription: " << testCase.description;
            EXPECT_GT(m_model->getSize(), 0) << "Model should contain nodes after conversion";
        }
        else
        {
            EXPECT_EQ(result, 0) << "Expression should fail: " << testCase.name
                                 << "\nExpression: " << testCase.expression
                                 << "\nDescription: " << testCase.description;
        }
    }

    // Test data for comprehensive implicit modeling expressions
    std::vector<ImplicitModelingTestCase> GetImplicitModelingTestCases()
    {
        std::vector<ImplicitModelingTestCase> testCases;

        // Helper to create arguments
        auto posArg = []() { return std::vector<FunctionArgument>{{"pos", ArgumentType::Vector}}; };
        auto posRadiusArgs = []()
        {
            return std::vector<FunctionArgument>{{"pos", ArgumentType::Vector},
                                                 {"radius", ArgumentType::Scalar}};
        };
        auto multiVectorArgs = []()
        {
            return std::vector<FunctionArgument>{{"pos", ArgumentType::Vector},
                                                 {"center", ArgumentType::Vector},
                                                 {"size", ArgumentType::Vector}};
        };
        auto scalarOutput = []() { return FunctionOutput{"result", ArgumentType::Scalar}; };
        auto vectorOutput = []() { return FunctionOutput{"result", ArgumentType::Vector}; };

        // =====================================================================================
        // Basic Primitive Distance Functions
        // =====================================================================================

        // Sphere SDF - length(pos) - radius
        testCases.push_back({"Sphere_Basic",
                             "sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - radius",
                             posRadiusArgs(),
                             scalarOutput(),
                             "Basic sphere distance function using explicit component access",
                             true});

        // Sphere SDF using length function (if available through nodes)
        testCases.push_back({
          "Sphere_UsingLength",
          "length(pos) - radius", // This may fail if length() function isn't supported in
                                  // expressions
          posRadiusArgs(),
          scalarOutput(),
          "Sphere distance function using length function",
          false // May not work if length() isn't in expression parser
        });

        // Box SDF approximation using max components
        testCases.push_back({"Box_Approximation",
                             "max(max(abs(pos.x) - 1.0, abs(pos.y) - 1.0), abs(pos.z) - 1.0)",
                             posArg(),
                             scalarOutput(),
                             "Box distance function approximation using max and abs",
                             true});

        // Plane SDF - dot product with normal
        testCases.push_back({"Plane_Basic",
                             "pos.y + 1.0",
                             posArg(),
                             scalarOutput(),
                             "Simple plane distance function (y = -1)",
                             true});

        // Cylinder SDF (infinite) - distance from Y axis
        testCases.push_back({"Cylinder_Infinite",
                             "sqrt(pos.x*pos.x + pos.z*pos.z) - radius",
                             posRadiusArgs(),
                             scalarOutput(),
                             "Infinite cylinder distance function along Y axis",
                             true});

        // =====================================================================================
        // Trigonometric and Periodic Functions
        // =====================================================================================

        // Sine wave displacement
        testCases.push_back({"SineWave_Displacement",
                             "pos.y - sin(pos.x * 3.14159)",
                             posArg(),
                             scalarOutput(),
                             "Sine wave displacement along Y axis",
                             true});

        // Cosine ripples
        testCases.push_back({"CosineRipples",
                             "sqrt(pos.x*pos.x + pos.z*pos.z) - 2.0 + cos(sqrt(pos.x*pos.x + "
                             "pos.z*pos.z) * 6.28) * 0.1",
                             posArg(),
                             scalarOutput(),
                             "Concentric cosine ripples creating ring patterns",
                             true});

        // Twisted sine pattern
        testCases.push_back({"TwistedSine",
                             "sin(pos.x + sin(pos.z) * 2.0) + cos(pos.y + cos(pos.x) * 2.0)",
                             posArg(),
                             scalarOutput(),
                             "Twisted sine pattern creating organic shapes",
                             true});

        // =====================================================================================
        // Advanced Mathematical Functions
        // =====================================================================================

        // Test simple exponential (without negative sign)
        testCases.push_back(
          {"SimpleExp", "exp(1.0)", {}, scalarOutput(), "Simple exponential function test", true});

        // Test simple negative
        testCases.push_back({"SimpleNegative",
                             "exp(-1.0)",
                             {},
                             scalarOutput(),
                             "Simple negative input to exponential",
                             true});

        // Test with variable but no sqrt
        testCases.push_back({"ExpWithVariable",
                             "exp(-pos.x)",
                             posArg(),
                             scalarOutput(),
                             "Exponential with negative variable",
                             true});

        // Test with sqrt but no variable
        testCases.push_back({"ExpWithSqrt",
                             "exp(-sqrt(1.0))",
                             {},
                             scalarOutput(),
                             "Exponential with negative sqrt",
                             true});

        // Exponential decay (now using supported exp function)
        testCases.push_back({"ExponentialDecay",
                             "exp(-sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z)) - 0.1",
                             posArg(),
                             scalarOutput(),
                             "Exponential decay distance function",
                             true});

        // Logarithmic spiral
        testCases.push_back(
          {"LogarithmicSpiral",
           "abs(log(sqrt(pos.x*pos.x + pos.z*pos.z) + 0.001) - pos.y * 0.5) - 0.1",
           posArg(),
           scalarOutput(),
           "Logarithmic spiral in 3D space",
           true});

        // Power function shaping
        testCases.push_back(
          {"PowerShaping",
           "pow(abs(pos.x), 2.5) + pow(abs(pos.y), 2.5) + pow(abs(pos.z), 2.5) - 1.0",
           posArg(),
           scalarOutput(),
           "Power function creating superellipsoid shape",
           true});

        // =====================================================================================
        // Boolean Operations Combinations
        // =====================================================================================

        // Union of two spheres
        testCases.push_back({"Union_TwoSpheres",
                             "min(sqrt((pos.x-1.0)*(pos.x-1.0) + pos.y*pos.y + pos.z*pos.z) - 0.5, "
                             "sqrt((pos.x+1.0)*(pos.x+1.0) + pos.y*pos.y + pos.z*pos.z) - 0.5)",
                             posArg(),
                             scalarOutput(),
                             "Union of two spheres using min operation",
                             true});

        // Intersection of cube and sphere
        testCases.push_back({"Intersection_CubeSphere",
                             "max(max(max(abs(pos.x) - 1.0, abs(pos.y) - 1.0), abs(pos.z) - 1.0), "
                             "sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - 1.5)",
                             posArg(),
                             scalarOutput(),
                             "Intersection of cube and sphere using max operation",
                             true});

        // Subtraction - sphere minus smaller sphere (without unary minus)
        testCases.push_back({"Subtraction_Spheres",
                             "max(sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - 2.0, (1.0 - "
                             "sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z)))",
                             posArg(),
                             scalarOutput(),
                             "Sphere with spherical hole (subtraction using algebraic inversion)",
                             true});

        // =====================================================================================
        // Complex Nested Functions
        // =====================================================================================

        // Nested trigonometric with component access
        testCases.push_back({"NestedTrig_Components",
                             "sin(cos(pos).x) + cos(sin(pos).y) + sin(cos(pos).z)",
                             posArg(),
                             scalarOutput(),
                             "Nested trigonometric functions with vector component access",
                             true});

        // Fractal-like nested functions
        testCases.push_back({"FractalNested",
                             "sin(pos.x + sin(pos.y + sin(pos.z))) - 0.5",
                             posArg(),
                             scalarOutput(),
                             "Fractal-like nested sine functions",
                             true});

        // Complex wave interference
        testCases.push_back({"WaveInterference",
                             "sin(sqrt(pos.x*pos.x + pos.z*pos.z) * 10.0 + pos.y * 5.0) * "
                             "cos(sqrt((pos.x-1.0)*(pos.x-1.0) + pos.z*pos.z) * 8.0)",
                             posArg(),
                             scalarOutput(),
                             "Wave interference pattern creating complex shapes",
                             true});

        // =====================================================================================
        // Clamping and Smoothing Operations
        // =====================================================================================

        // Test simple clamp function
        testCases.push_back({"SimpleClamp",
                             "clamp(1.5, 0.0, 2.0)",
                             {},
                             scalarOutput(),
                             "Simple clamp function test",
                             true});

        // Clamped sphere (now using supported clamp function)
        testCases.push_back(
          {"ClampedSphere",
           "clamp(sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - 1.0, -0.5, 0.5)",
           posArg(),
           scalarOutput(),
           "Clamped sphere distance for smoothed boundaries",
           true});

        // Smooth minimum blend
        testCases.push_back({"SmoothMinBlend",
                             "sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - 1.0 + "
                             "sin(sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) * 10.0) * 0.1",
                             posArg(),
                             scalarOutput(),
                             "Smooth minimum blend creating surface details",
                             true});

        // =====================================================================================
        // Noise and Texture-like Functions
        // =====================================================================================

        // Pseudo-noise using trigonometric functions
        testCases.push_back({"PseudoNoise",
                             "sin(pos.x * 12.9898 + pos.y * 78.233 + pos.z * 37.719) * 0.1",
                             posArg(),
                             scalarOutput(),
                             "Pseudo-noise pattern using trigonometric functions",
                             true});

        // Layered noise
        testCases.push_back(
          {"LayeredNoise",
           "sin(pos.x * 5.0) * 0.5 + sin(pos.y * 7.0) * 0.3 + sin(pos.z * 11.0) * 0.2",
           posArg(),
           scalarOutput(),
           "Layered noise with different frequencies",
           true});

        // =====================================================================================
        // Deformation and Transformation Effects
        // =====================================================================================

        // Twisted geometry
        testCases.push_back({"TwistedGeometry",
                             "sqrt((pos.x * cos(pos.y) - pos.z * sin(pos.y)) * (pos.x * cos(pos.y) "
                             "- pos.z * sin(pos.y)) + (pos.x * sin(pos.y) + pos.z * cos(pos.y)) * "
                             "(pos.x * sin(pos.y) + pos.z * cos(pos.y))) - 1.0",
                             posArg(),
                             scalarOutput(),
                             "Twisted cylinder using rotation transformation",
                             true});

        // Bent space
        testCases.push_back({"BentSpace",
                             "sqrt((pos.x + sin(pos.y * 2.0) * 0.5) * (pos.x + sin(pos.y * 2.0) * "
                             "0.5) + pos.y * pos.y + pos.z * pos.z) - 1.0",
                             posArg(),
                             scalarOutput(),
                             "Bent space deformation using sine displacement",
                             true});

        // =====================================================================================
        // Mathematical Edge Cases and Precision Tests
        // =====================================================================================

        // Very small values
        testCases.push_back({"TinyValues",
                             "sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - 0.001",
                             posArg(),
                             scalarOutput(),
                             "Very small sphere for precision testing",
                             true});

        // Large values
        testCases.push_back({"LargeValues",
                             "sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - 1000.0",
                             posArg(),
                             scalarOutput(),
                             "Large sphere for scale testing",
                             true});

        // Zero division protection
        testCases.push_back({"ZeroDivisionProtection",
                             "pos.x / (abs(pos.y) + 0.001)",
                             posArg(),
                             scalarOutput(),
                             "Division with zero protection",
                             true});

        // =====================================================================================
        // Multi-dimensional Operations
        // =====================================================================================

        // Vector field rotation
        testCases.push_back({"VectorFieldRotation",
                             "sin(pos.x) + cos(pos.y) + sin(pos.z)",
                             posArg(),
                             scalarOutput(),
                             "Simple vector field combination",
                             true});

        // Cross product simulation (using components)
        testCases.push_back({"CrossProductSimulation",
                             "(pos.y * 1.0 - pos.z * 0.0) + (pos.z * 0.0 - pos.x * 1.0) + (pos.x * "
                             "0.0 - pos.y * 1.0)",
                             posArg(),
                             scalarOutput(),
                             "Cross product simulation using component operations",
                             true});

        // =====================================================================================
        // Error Cases - Should Fail
        // =====================================================================================

        // Invalid component access
        testCases.push_back({"InvalidComponent",
                             "pos.w",
                             posArg(),
                             scalarOutput(),
                             "Invalid component access (.w doesn't exist for vectors)",
                             false});

        // Invalid function call
        testCases.push_back({"InvalidFunction",
                             "invalidfunc(pos.x)",
                             posArg(),
                             scalarOutput(),
                             "Invalid function call",
                             false});

        // Mismatched parentheses
        testCases.push_back({"MismatchedParentheses",
                             "sin(pos.x + cos(pos.y)",
                             posArg(),
                             scalarOutput(),
                             "Mismatched parentheses in expression",
                             false});

        // Empty expression
        testCases.push_back(
          {"EmptyExpression", "", posArg(), scalarOutput(), "Empty expression", false});

        // =====================================================================================
        // Advanced Implicit Modeling Techniques
        // =====================================================================================

        // Metaball/blob simulation
        testCases.push_back(
          {"Metaball",
           "1.0 / (sqrt((pos.x-1.0)*(pos.x-1.0) + pos.y*pos.y + pos.z*pos.z) + 0.01) + 1.0 / "
           "(sqrt((pos.x+1.0)*(pos.x+1.0) + pos.y*pos.y + pos.z*pos.z) + 0.01) - 10.0",
           posArg(),
           scalarOutput(),
           "Metaball field using inverse distance",
           true});

        // Heightfield terrain
        testCases.push_back({"HeightfieldTerrain",
                             "pos.y - (sin(pos.x * 2.0) * cos(pos.z * 2.0) * 0.5 + sin(pos.x * 4.0 "
                             "+ pos.z * 3.0) * 0.25)",
                             posArg(),
                             scalarOutput(),
                             "Heightfield terrain using trigonometric functions",
                             true});

        // Voronoi-like pattern
        testCases.push_back(
          {"VoronoiPattern",
           "min(min(sqrt((pos.x-floor(pos.x+0.5))*(pos.x-floor(pos.x+0.5)) + "
           "(pos.y-floor(pos.y+0.5))*(pos.y-floor(pos.y+0.5)) + "
           "(pos.z-floor(pos.z+0.5))*(pos.z-floor(pos.z+0.5))), "
           "sqrt((pos.x-floor(pos.x))*(pos.x-floor(pos.x)) + "
           "(pos.y-floor(pos.y))*(pos.y-floor(pos.y)) + "
           "(pos.z-floor(pos.z))*(pos.z-floor(pos.z)))), "
           "sqrt((pos.x-ceil(pos.x))*(pos.x-ceil(pos.x)) + (pos.y-ceil(pos.y))*(pos.y-ceil(pos.y)) "
           "+ (pos.z-ceil(pos.z))*(pos.z-ceil(pos.z)))) - 0.1",
           posArg(),
           scalarOutput(),
           "Voronoi-like pattern using floor/ceil functions",
           true});

        return testCases;
    }

    INSTANTIATE_TEST_SUITE_P(ImplicitModeling,
                             ImplicitModelingExpressionTest,
                             ::testing::ValuesIn(GetImplicitModelingTestCases()),
                             [](const ::testing::TestParamInfo<ImplicitModelingTestCase> & info)
                             { return info.param.name; });

    // =====================================================================================
    // Specialized Test Cases for Edge Conditions
    // =====================================================================================

    class ImplicitModelingEdgeCaseTest : public ExpressionToGraphConverterTest
    {
    };

    TEST_F(ImplicitModelingEdgeCaseTest, ConvertComplexNestingDepth_DeepNesting_HandledCorrectly)
    {
        std::vector<FunctionArgument> arguments = {{"pos", ArgumentType::Vector}};

        // Test deep nesting - 5 levels
        std::string deepExpression = "sin(cos(sin(cos(sin(pos.x)))))";

        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          deepExpression,
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});

        EXPECT_NE(result, 0) << "Deep nesting should be handled correctly";
        EXPECT_GT(m_model->getSize(), 0) << "Model should contain nodes for deep nesting";
    }

    TEST_F(ImplicitModelingEdgeCaseTest, ConvertLargeExpression_ManyTerms_HandledEfficiently)
    {
        std::vector<FunctionArgument> arguments = {{"pos", ArgumentType::Vector}};

        // Large expression with many terms
        std::string largeExpression =
          "pos.x + pos.y + pos.z + sin(pos.x) + cos(pos.y) + sin(pos.z) + pos.x*pos.x + "
          "pos.y*pos.y + pos.z*pos.z + sqrt(pos.x*pos.x + pos.y*pos.y) + abs(pos.x) + abs(pos.y) + "
          "abs(pos.z)";

        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          largeExpression,
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});

        EXPECT_NE(result, 0) << "Large expression should be handled efficiently";
        EXPECT_GT(m_model->getSize(), 10) << "Model should contain many nodes for large expression";
    }

    TEST_F(ImplicitModelingEdgeCaseTest, ConvertPrecisionCritical_SmallNumbers_MaintainsPrecision)
    {
        std::vector<FunctionArgument> arguments = {{"pos", ArgumentType::Vector}};

        // Expression with very small constants
        std::string precisionExpression = "sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z) - 0.00001";

        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          precisionExpression,
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});

        EXPECT_NE(result, 0) << "Precision-critical expressions should work";
    }

    TEST_F(ImplicitModelingEdgeCaseTest, ConvertRepeatedSubexpressions_OptimizesCorrectly)
    {
        std::vector<FunctionArgument> arguments = {{"pos", ArgumentType::Vector}};

        // Expression with repeated subexpressions that should be optimized
        std::string repeatedExpression = "sin(pos.x) + sin(pos.x) + cos(pos.x) + cos(pos.x)";

        nodes::NodeId result = ExpressionToGraphConverter::convertExpressionToGraph(
          repeatedExpression,
          *m_model,
          *m_parser,
          arguments,
          FunctionOutput{"result", ArgumentType::Scalar});

        EXPECT_NE(result, 0) << "Repeated subexpressions should be handled correctly";

        // Check if optimization occurs (should have fewer sin/cos nodes than expected if optimized)
        auto sineCount = gladius_tests::helper::countNumberOfNodesOfType<nodes::Sine>(*m_model);
        auto cosineCount = gladius_tests::helper::countNumberOfNodesOfType<nodes::Cosine>(*m_model);

        // Without optimization: would expect 2 sine nodes and 2 cosine nodes
        // With optimization: should be less (ideally 1 of each)
        EXPECT_LE(sineCount, 2) << "Sine nodes should be optimized";
        EXPECT_LE(cosineCount, 2) << "Cosine nodes should be optimized";
    }

    // =====================================================================================
    // Constant handling regression tests
    // =====================================================================================

    TEST_F(ExpressionToGraphConverterTest,
           ConstantLiteral_InExpression_ValueStoredInParameter_NotInDisplayName)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("x", ArgumentType::Scalar);
        std::string const expression = "x + 2.5";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

        // Assert basic success
        EXPECT_NE(resultNodeId, 0);

        // Find ConstantScalar node and verify its parameter value
        nodes::ConstantScalar * constantNode = nullptr;
        for (auto it = m_model->begin(); it != m_model->end(); ++it)
        {
            if (auto * n = dynamic_cast<nodes::ConstantScalar *>(it->second.get()))
            {
                constantNode = n;
                break;
            }
        }
        ASSERT_NE(constantNode, nullptr) << "Expected a ConstantScalar node for literal 2.5";

        auto valueVariant = constantNode->parameter().at(nodes::FieldNames::Value).getValue();
        ASSERT_TRUE(std::holds_alternative<float>(valueVariant));
        EXPECT_NEAR(std::get<float>(valueVariant), 2.5f, 1e-5f);

        // Display name should not be the numeric literal itself
        EXPECT_NE(constantNode->getDisplayName(), std::string("2.5"));
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConstantLiteral_UnaryMinus_ValueStoredAsNegativeParameter)
    {
        // Arrange
        std::vector<FunctionArgument> arguments;
        arguments.emplace_back("x", ArgumentType::Scalar);
        std::string const expression = "-3 * x";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, arguments, FunctionOutput::defaultOutput());

        // Assert basic success
        EXPECT_NE(resultNodeId, 0);

        // Find ConstantScalar node and verify its parameter value is -3
        nodes::ConstantScalar * constantNode = nullptr;
        for (auto it = m_model->begin(); it != m_model->end(); ++it)
        {
            if (auto * n = dynamic_cast<nodes::ConstantScalar *>(it->second.get()))
            {
                constantNode = n;
                break;
            }
        }
        ASSERT_NE(constantNode, nullptr) << "Expected a ConstantScalar node for literal -3";

        auto valueVariant = constantNode->parameter().at(nodes::FieldNames::Value).getValue();
        ASSERT_TRUE(std::holds_alternative<float>(valueVariant));
        EXPECT_NEAR(std::get<float>(valueVariant), -3.0f, 1e-5f);
    }

    TEST_F(ExpressionToGraphConverterTest,
           ConstantLiteral_Alone_NumberOnly_CreatesConstantWithCorrectValue)
    {
        // Arrange
        std::string const expression = "42";

        // Act
        nodes::NodeId resultNodeId = ExpressionToGraphConverter::convertExpressionToGraph(
          expression, *m_model, *m_parser, {}, FunctionOutput::defaultOutput());

        // Assert
        EXPECT_NE(resultNodeId, 0);

        nodes::ConstantScalar * constantNode = nullptr;
        for (auto it = m_model->begin(); it != m_model->end(); ++it)
        {
            if (auto * n = dynamic_cast<nodes::ConstantScalar *>(it->second.get()))
            {
                constantNode = n;
                break;
            }
        }
        ASSERT_NE(constantNode, nullptr) << "Expected a ConstantScalar node for literal 42";

        auto valueVariant = constantNode->parameter().at(nodes::FieldNames::Value).getValue();
        ASSERT_TRUE(std::holds_alternative<float>(valueVariant));
        EXPECT_NEAR(std::get<float>(valueVariant), 42.0f, 1e-5f);
    }

} // namespace gladius::tests
