/**
 * @file FunctionGraphRoundTrip_tests.cpp
 * @brief Tests round-trip using serializeMinimal -> setFunctionGraph importer.
 */

#include <gtest/gtest.h>

#include "ExpressionParser.h"
#include "ExpressionToGraphConverter.h"
#include "FunctionArgument.h"
#include "mcp/FunctionGraphDeserializer.h"
#include "mcp/FunctionGraphSerializer.h"
#include "nodes/Model.h"

using gladius::FunctionGraphSerializer;
using gladius::mcp::FunctionGraphDeserializer;

namespace gladius::tests
{
    /**
     * [FunctionGraph_MinimalSerializeThenSet_RoundTripPreservesTopology]
     * Arrange: Create a simple model with Constant -> End.Shape
     * Act: Serialize with serializeMinimal, import into a fresh model with
     * applyToModel(replace=true) Assert: Import succeeds, one Constant exists, and End.Shape is
     * linked to Constant.Value
     */
    TEST(FunctionGraph_MinimalSerializeThenSet_RoundTripPreservesTopology, Basic)
    {
        // Arrange: source model with Begin/End and one Constant feeding Shape
        gladius::nodes::Model source;
        source.createBeginEndWithDefaultInAndOuts();

        auto * constant = source.create<gladius::nodes::ConstantScalar>();

        auto & valuePort = constant->getOutputs().at(gladius::nodes::FieldNames::Value);
        auto * shapeParam = source.getEndNode()->getParameter(gladius::nodes::FieldNames::Shape);
        ASSERT_NE(shapeParam, nullptr);
        bool linked = source.addLink(valuePort.getId(), shapeParam->getId());
        ASSERT_TRUE(linked);
        source.updateGraphAndOrderIfNeeded();

        // Act: minimal serialize, then apply to a fresh model
        auto minimal = FunctionGraphSerializer::serializeMinimal(source);

        gladius::nodes::Model target;
        auto result = FunctionGraphDeserializer::applyToModel(target, minimal, /*replace=*/true);

        // Assert: success and expected topology
        ASSERT_TRUE(result.contains("success"));
        EXPECT_TRUE(result["success"].get<bool>()) << result.dump();

        // Stricter: node count and types (Input, Output, ConstantScalar)
        ASSERT_TRUE(minimal.contains("counts"));
        auto serializedNodeCount = minimal["counts"]["nodes"].get<size_t>();
        EXPECT_EQ(target.getSize(), serializedNodeCount);

        // Collect target node type names
        std::set<std::string> nodeTypes;
        for (auto it = target.begin(); it != target.end(); ++it)
        {
            if (auto * n = it->second.get())
                nodeTypes.insert(n->name());
        }
        EXPECT_TRUE(nodeTypes.count("Input") == 1 || nodeTypes.count("Begin") == 1);
        EXPECT_TRUE(nodeTypes.count("Output") == 1 || nodeTypes.count("End") == 1);
        EXPECT_EQ(nodeTypes.count("ConstantScalar"), 1u);

        // End.Shape should have a source from a Constant.Value
        auto * endParam = target.getEndNode()->getParameter(gladius::nodes::FieldNames::Shape);
        ASSERT_NE(endParam, nullptr);
        auto src = endParam->getConstSource();
        ASSERT_TRUE(src.has_value());

        auto parentNodeOpt = target.getNode(src->nodeId);
        ASSERT_TRUE(parentNodeOpt.has_value());
        auto * parentNode = parentNodeOpt.value();

        // Node name should be "ConstantScalar" and output short name should be Value
        EXPECT_EQ(parentNode->name(), std::string("ConstantScalar"));
        EXPECT_EQ(src->shortName, gladius::nodes::FieldNames::Value);

        // End.Shape type must be float
        EXPECT_EQ(endParam->getTypeIndex(), gladius::nodes::ParameterTypeIndex::Float);

        // Stricter: link count parity with serialization
        size_t serializedLinks = minimal["counts"]["links"].get<size_t>();
        size_t targetLinks = 0;
        for (auto it = target.begin(); it != target.end(); ++it)
        {
            auto * node = it->second.get();
            if (!node)
                continue;
            for (auto const & kv : node->constParameter())
            {
                if (kv.second.getConstSource().has_value())
                    ++targetLinks;
            }
        }
        EXPECT_EQ(targetLinks, serializedLinks);

        // Stricter: id_map type consistency (client type -> target node->name())
        ASSERT_TRUE(result.contains("id_map"));
        auto const & idMap = result["id_map"];
        // Build source mapping from client id to type
        std::unordered_map<std::string, std::string> sourceTypes;
        for (auto const & jn : minimal["nodes"])
        {
            sourceTypes[std::to_string(jn["id"].get<uint32_t>())] = jn["type"].get<std::string>();
        }
        for (auto it = idMap.begin(); it != idMap.end(); ++it)
        {
            auto clientIdStr = it.key();
            auto targetNodeId = it.value().get<uint32_t>();
            auto typeIt = sourceTypes.find(clientIdStr);
            ASSERT_TRUE(typeIt != sourceTypes.end());
            auto targetNodeOpt = target.getNode(targetNodeId);
            ASSERT_TRUE(targetNodeOpt.has_value());
            EXPECT_EQ(targetNodeOpt.value()->name(), typeIt->second);
        }
    }

    /**
     * [FunctionGraph_ExpressionRoundTrip_Gyroid_Minimal]
     * Arrange: Create model from a gyroid-like expression using pos.x/y/z, output float "shape"
     * Act: Serialize minimal and import into a fresh model (replace=true)
     * Assert: Import succeeds and End.Shape is connected; at least one Sine or Cosine node exists
     */
    TEST(FunctionGraph_ExpressionRoundTrip_Gyroid_Minimal, ComplexGraph)
    {
        using namespace gladius;
        using namespace gladius::nodes;

        // Arrange: build a model from an expression via converter
        nodes::Model source;
        source.createBeginEndWithDefaultInAndOuts();

        std::string expr = "sin(pos.x)*cos(pos.y) + sin(pos.y)*cos(pos.z) + sin(pos.z)*cos(pos.x)";
        ExpressionParser parser;
        ASSERT_TRUE(parser.parseExpression(expr)) << parser.getLastError();

        std::vector<FunctionArgument> args;
        args.emplace_back("pos", ArgumentType::Vector);

        FunctionOutput out;
        out.name = "shape";
        out.type = ArgumentType::Scalar;

        auto resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expr, source, parser, args, out);
        ASSERT_NE(resultNodeId, 0) << "Expression conversion failed";
        source.updateGraphAndOrderIfNeeded();

        // Act: minimal serialize, then import
        auto minimal = FunctionGraphSerializer::serializeMinimal(source);

        nodes::Model target;
        auto result =
          mcp::FunctionGraphDeserializer::applyToModel(target, minimal, /*replace=*/true);
        ASSERT_TRUE(result.contains("success"));
        EXPECT_TRUE(result["success"].get<bool>()) << result.dump();

        // Assert: End.shape is connected
        auto * endParam = target.getEndNode()->getParameter(FieldNames::Shape);
        ASSERT_NE(endParam, nullptr);
        EXPECT_TRUE(endParam->getConstSource().has_value());

        // Stricter: End.Shape type must be float
        EXPECT_EQ(endParam->getTypeIndex(), nodes::ParameterTypeIndex::Float);

        // Stricter: link count parity with serialization
        ASSERT_TRUE(minimal.contains("counts"));
        size_t serializedLinks = minimal["counts"]["links"].get<size_t>();
        size_t targetLinks = 0;
        for (auto it = target.begin(); it != target.end(); ++it)
        {
            auto * node = it->second.get();
            if (!node)
                continue;
            for (auto const & kv : node->constParameter())
            {
                if (kv.second.getConstSource().has_value())
                    ++targetLinks;
            }
        }
        EXPECT_EQ(targetLinks, serializedLinks);

        // And at least one Sine or Cosine node exists in the round-tripped model
        bool hasSin = false;
        bool hasCos = false;
        for (auto it = target.begin(); it != target.end(); ++it)
        {
            auto * node = it->second.get();
            if (!node)
                continue;
            auto n = node->name();
            if (n == "Sine")
                hasSin = true;
            if (n == "Cosine")
                hasCos = true;
        }
        EXPECT_TRUE(hasSin) << "Expected at least one Sine node in graph";
        EXPECT_TRUE(hasCos) << "Expected at least one Cosine node in graph";

        // Stricter: have at least one addition and one multiplication in gyroid
        bool hasAdd = false, hasMul = false;
        for (auto it = target.begin(); it != target.end(); ++it)
        {
            auto * node = it->second.get();
            if (!node)
                continue;
            auto n = node->name();
            if (n == "Addition")
                hasAdd = true;
            if (n == "Multiplication")
                hasMul = true;
        }
        EXPECT_TRUE(hasAdd);
        EXPECT_TRUE(hasMul);
    }

    /**
     * [FunctionGraph_ExpressionRoundTrip_NestedFunctions]
     * Arrange: Build a more complex nested expression using pos.x and constants
     * Act: Round-trip via minimal serializer/deserializer
     * Assert: Success and End.Shape has a source; some arithmetic node exists
     */
    TEST(FunctionGraph_ExpressionRoundTrip_NestedFunctions, ComplexGraph)
    {
        using namespace gladius;
        using namespace gladius::nodes;

        nodes::Model source;
        source.createBeginEndWithDefaultInAndOuts();

        // Nested/tricky expression with pow/sin/add/mul
        std::string expr =
          "pow(sin(pos.x*2*pi/10),2) + sin(pos.y*2*pi/10)*cos(pos.z*2*pi/10) - 0.25";
        ExpressionParser parser;
        ASSERT_TRUE(parser.parseExpression(expr)) << parser.getLastError();

        std::vector<FunctionArgument> args;
        args.emplace_back("pos", ArgumentType::Vector);

        FunctionOutput out;
        out.name = "shape";
        out.type = ArgumentType::Scalar;

        auto resultNodeId =
          ExpressionToGraphConverter::convertExpressionToGraph(expr, source, parser, args, out);
        ASSERT_NE(resultNodeId, 0) << "Expression conversion failed";
        source.updateGraphAndOrderIfNeeded();

        auto minimal = FunctionGraphSerializer::serializeMinimal(source);

        nodes::Model target;
        auto result =
          mcp::FunctionGraphDeserializer::applyToModel(target, minimal, /*replace=*/true);
        ASSERT_TRUE(result.contains("success"));
        EXPECT_TRUE(result["success"].get<bool>()) << result.dump();

        auto * endParam = target.getEndNode()->getParameter(FieldNames::Shape);
        ASSERT_NE(endParam, nullptr);
        EXPECT_TRUE(endParam->getConstSource().has_value());

        // Stricter: End.Shape type must be float
        EXPECT_EQ(endParam->getTypeIndex(), nodes::ParameterTypeIndex::Float);

        // Stricter: link count parity with serialization
        ASSERT_TRUE(minimal.contains("counts"));
        size_t serializedLinks = minimal["counts"]["links"].get<size_t>();
        size_t targetLinks = 0;
        for (auto it = target.begin(); it != target.end(); ++it)
        {
            auto * node = it->second.get();
            if (!node)
                continue;
            for (auto const & kv : node->constParameter())
            {
                if (kv.second.getConstSource().has_value())
                    ++targetLinks;
            }
        }
        EXPECT_EQ(targetLinks, serializedLinks);

        // Expect at least one arithmetic node (Addition, Multiplication, Subtraction, Division)
        bool hasArithmetic = false;
        bool hasPow = false;
        bool hasConstant = false;
        for (auto it = target.begin(); it != target.end(); ++it)
        {
            auto * node = it->second.get();
            if (!node)
                continue;
            auto n = node->name();
            if (n == "Addition" || n == "Multiplication" || n == "Subtraction" || n == "Division" ||
                n == "Pow")
            {
                hasArithmetic = true;
            }
            if (n == "Pow")
                hasPow = true;
            if (n == "ConstantScalar")
                hasConstant = true;
        }
        EXPECT_TRUE(hasArithmetic) << "Expected arithmetic nodes in complex graph";
        EXPECT_TRUE(hasPow) << "Expected Pow node in complex graph";
        EXPECT_TRUE(hasConstant) << "Expected at least one ConstantScalar node";
    }
} // namespace gladius::tests
