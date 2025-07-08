#include <gtest/gtest.h>
#include <nodes/Model.h>

#include "testhelper.h"
#include <nodes/NodeBase.h>
#include <nodes/nodesfwd.h>

namespace gladius_tests
{

    class Model : public ::testing::Test
    {
        void SetUp() override
        {
        }
    };

    TEST_F(Model, CreeateAddition_ModelContainsAddition)
    {
        nodes::Model model;
        model.create<nodes::Addition>();
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Addition>(model), 1);
    }

    TEST_F(Model, CreateBeginEnd_ModelContainsBeginAndEnd)
    {
        nodes::Model model;
        model.createBeginEnd();
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Begin>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::End>(model), 1);
    }

    TEST_F(Model, Create_AnyNodeType_ModelContainsNodeOfType)
    {
        nodes::Model model;
        nodes::NodeTypes nodeTypes;

        nodes::staticFor(nodeTypes, [&](auto i, auto & node) {
            (void) i;
            model.create(node);
            EXPECT_EQ(helper::countNumberOfNodesOfType<decltype(node)>(model), 1);
        });
    }

    TEST_F(Model, Create_MultipleNodesPerType_ModelContainsNodeOfType)
    {
        nodes::Model model;
        nodes::NodeTypes nodeTypes;
        auto constexpr numberOfNodes = 10;
        nodes::staticFor(nodeTypes, [&](auto i, auto & node) {
            (void) i;
            for (int j = 0; j < numberOfNodes; ++j)
            {
                model.create(node);
            }
            EXPECT_EQ(helper::countNumberOfNodesOfType<decltype(node)>(model), numberOfNodes);
        });
    }

    TEST_F(Model, Remove_idOfAddedNode_nodeRemoved)
    {
        nodes::Model model;
        model.createBeginEnd();

        auto nodeId = model.create<nodes::Addition>()->getId();
        EXPECT_TRUE(model.getNode(nodeId).has_value());

        auto visitor = gladius::nodes::OnTypeVisitor<nodes::Addition>(
          [&](auto & visitedNode) { EXPECT_EQ(visitedNode.getId(), nodeId); });
        model.visitNodes(visitor);

        model.remove(nodeId);

        EXPECT_FALSE(model.getNode(nodeId).has_value());
    }

    TEST_F(Model, AddArgument_NewArgument_BeginNodeHasNewInputAndOutput)
    {
        nodes::Model model;
        model.createBeginEnd();
        nodes::ParameterName newArgument{"NewArgument"};
        auto newArgInitalValue = nodes::VariantParameter{1.234f};
        model.addArgument(newArgument, newArgInitalValue);

        auto & ports = model.getPortRegistry();

        auto iter = std::find_if(std::begin(ports), std::end(ports), [&](auto port) {
            return port.second->getShortName() == newArgument;
        });

        EXPECT_FALSE(iter == std::end(ports));
        EXPECT_EQ(iter->second->getParentId(), model.getBeginNode()->getId());
    }

/**
 * @brief Test case to verify that a dependency is added when a link is added between the output of the begin node and the input of an addition node.
 * 
 * Test case:
 * 1. Create a model with a begin node and an addition node.
 * 2. Add a link between the output of the begin node and the input of the addition node.
 * 3. Verify that the addition node's input has the begin node's output as its source.
 * 4. Verify that the addition node is directly depending on the begin node in the graph.
 */
    TEST_F(Model, AddLink_AdditionNode_DependencyAdded)
    {
        // Arrange
        nodes::Model model;
        model.createBeginEndWithDefaultInAndOuts();
        auto const beginNodeId = model.getBeginNode()->getId();
        auto const beginNodeCsId =
          model.getBeginNode()->getOutputs().at(nodes::FieldNames::Pos).getId();
        auto node = model.create<nodes::Addition>();
        auto const additionId = node->getId();
        auto const additionAInputId =
          node->parameter().at(nodes::FieldNames::A).getId();

        // Act
        EXPECT_TRUE(model.addLink(beginNodeCsId, additionAInputId));
        model.updateGraphAndOrderIfNeeded();
        // ASSERT

        EXPECT_TRUE(node->parameter().at(nodes::FieldNames::A).getSource().has_value());
        EXPECT_EQ(
          node->parameter().at(nodes::FieldNames::A).getSource().value().portId,
          beginNodeCsId);

        EXPECT_EQ(
          node->parameter().at(nodes::FieldNames::A).getSource().value().uniqueName,
          model.getBeginNode()->getOutputs().at(nodes::FieldNames::Pos).getUniqueName());

        auto & graph = model.getGraph();
        EXPECT_LT(additionId, graph.getSize());
        EXPECT_LT(beginNodeId, graph.getSize());
        EXPECT_TRUE(graph.isDirectlyDependingOn(additionId, beginNodeId));
    }

    TEST_F(Model, RemoveLink_AdditionNode_DependencyIsRemoved)
    {
        // Arrange
        nodes::Model model;
        model.createBeginEndWithDefaultInAndOuts();
        auto const beginNodeId = model.getBeginNode()->getId();
        
         auto const beginNodeCsId =
          model.getBeginNode()->getOutputs().at(nodes::FieldNames::Pos).getId();
        auto const additionNode = model.create<nodes::Addition>();
        auto const nodeId = additionNode->getId();
        auto const additionAInputId =
          additionNode->parameter().at(nodes::FieldNames::A).getId();

        model.addLink(beginNodeCsId, additionAInputId);
        model.updateGraphAndOrderIfNeeded();
        // Act
        model.removeLink(beginNodeCsId, additionAInputId);

        // ASSERT
        EXPECT_FALSE(additionNode->parameter().at(nodes::FieldNames::A).getSource().has_value());

        model.updateGraphAndOrderIfNeeded();
        auto & graph = model.getGraph();
        EXPECT_LT(nodeId, graph.getSize());
        EXPECT_LT(beginNodeId, graph.getSize());
        EXPECT_FALSE(graph.isDirectlyDependingOn(nodeId, beginNodeId));
    }

    // Copy constructor
    TEST_F(Model, CopyConstructor_ModelWithAddition_ModelContainsAddition)
    {
        nodes::Model model;
        model.create<nodes::Addition>();
        nodes::Model copy{model};
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Addition>(copy), 1);
    }

    TEST_F(Model, SimplifyModel_NodesNotConnectedToEnd_AreRemoved)
    {
        // Arrange
        nodes::Model model;
        model.createBeginEndWithDefaultInAndOuts(); // Use the method that creates default parameters
        
        auto* beginNode = model.getBeginNode();
        auto* endNode = model.getEndNode();
        
        // Create nodes that are connected to the end node
        auto* connectedMath = model.create<nodes::Addition>();
        auto* connectedScalar = model.create<nodes::ConstantScalar>();
        
        // Create nodes that are disconnected from the end node
        auto* disconnectedMath = model.create<nodes::Subtraction>();
        auto* disconnectedSine = model.create<nodes::Sine>();
        
        // Set up connections for the connected nodes
        beginNode->addOutputPort("value", nodes::ParameterTypeIndex::Float);
        model.registerOutputs(*beginNode); // Register the new output port
        
        // Get port and parameter IDs for creating links
        auto beginValuePortId = beginNode->getOutputs().at("value").getId();
        auto mathAParamId = connectedMath->parameter().at(nodes::FieldNames::A).getId();
        auto mathResultPortId = connectedMath->getOutputs().at(nodes::FieldNames::Result).getId();
        auto shapeParamId = endNode->parameter().at(nodes::FieldNames::Shape).getId();
        
        // Set up a proper connection chain: Begin -> Addition -> End
        model.addLink(beginValuePortId, mathAParamId);
        connectedMath->parameter()[nodes::FieldNames::B] = nodes::VariantParameter(1.0f);
        
        // Link addition result to end node shape parameter
        model.addLink(mathResultPortId, shapeParamId);
        
        // Set a value for the constant scalar (it's not connected to anything)
        connectedScalar->parameter()[nodes::FieldNames::Value] = nodes::VariantParameter(2.0f);
        
        // Count initial nodes (should be 6: Begin, End, 2 connected, 2 disconnected)
        int initialNodeCount = 0;
        for (auto it = model.begin(); it != model.end(); ++it) {
            ++initialNodeCount;
        }
        EXPECT_EQ(initialNodeCount, 6);
        
        // Act
        size_t removedCount = model.simplifyModel();
        
        // Assert
        EXPECT_EQ(removedCount, 3); // Three disconnected nodes should be removed (ConstantScalar, Subtraction, Sine)
        
        // Count remaining nodes (should be 3: Begin, End, Addition)
        int finalNodeCount = 0;
        for (auto it = model.begin(); it != model.end(); ++it) {
            ++finalNodeCount;
        }
        EXPECT_EQ(finalNodeCount, 3); 
        
        // Verify that the disconnected nodes were removed
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Subtraction>(model), 0);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Sine>(model), 0);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::ConstantScalar>(model), 0);
        
        // Verify that the connected nodes still exist
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Addition>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Begin>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::End>(model), 1);
    }
    
    TEST_F(Model, SimplifyModel_ComplexGraph_RemovesOnlyDisconnectedNodes)
    {
        // Arrange
        nodes::Model model;
        model.createBeginEndWithDefaultInAndOuts();
        
        // Create a more complex graph with both connected and disconnected components
        
        // Connected path 1: Begin -> Add -> Multiply -> End
        auto* add = model.create<nodes::Addition>();
        auto* multiply = model.create<nodes::Multiplication>();
        
        // Connected path 2: Begin -> Subtract -> ComposeVector -> End
        auto* subtract = model.create<nodes::Subtraction>();
        auto* composeVector = model.create<nodes::ComposeVector>();
        
        // Disconnected path: Divide -> Sine -> Cosine
        auto* divide = model.create<nodes::Division>();
        auto* sine = model.create<nodes::Sine>();
        auto* cosine = model.create<nodes::Cosine>();
        
        // Set up connections for the first connected path
        model.getBeginNode()->addOutputPort("value1", nodes::ParameterTypeIndex::Float);
        model.registerOutputs(*model.getBeginNode());
        
        // Get port and parameter IDs for the first connected path
        auto begin1PortId = model.getBeginNode()->getOutputs().at("value1").getId();
        auto addAParamId = add->parameter().at(nodes::FieldNames::A).getId();
        auto addResultPortId = add->getOutputs().at(nodes::FieldNames::Result).getId();
        auto multiplyAParamId = multiply->parameter().at(nodes::FieldNames::A).getId();
        auto multiplyResultPortId = multiply->getOutputs().at(nodes::FieldNames::Result).getId();
        
        // Add parameter to End node for value1
        model.addFunctionOutput("value1", nodes::VariantParameter(0.0f));
        auto value1ParamId = model.getEndNode()->parameter().at("value1").getId();
        
        // Begin -> Add
        model.addLink(begin1PortId, addAParamId);
        add->parameter()[nodes::FieldNames::B] = nodes::VariantParameter(1.0f);
        
        // Add -> Multiply
        model.addLink(addResultPortId, multiplyAParamId);
        multiply->parameter()[nodes::FieldNames::B] = nodes::VariantParameter(2.0f);
        
        // Multiply -> End
        model.addLink(multiplyResultPortId, value1ParamId);
        
        // Set up connections for the second connected path
        model.getBeginNode()->addOutputPort("value2", nodes::ParameterTypeIndex::Float);
        model.registerOutputs(*model.getBeginNode());
        
        // Get port and parameter IDs for the second connected path
        auto begin2PortId = model.getBeginNode()->getOutputs().at("value2").getId();
        auto subtractAParamId = subtract->parameter().at(nodes::FieldNames::A).getId();
        auto subtractResultPortId = subtract->getOutputs().at(nodes::FieldNames::Result).getId();
        auto vectorXParamId = composeVector->parameter().at("x").getId();
        auto vectorResultPortId = composeVector->getOutputs().at(nodes::FieldNames::Result).getId();
        auto shapeParamId = model.getEndNode()->parameter().at(nodes::FieldNames::Shape).getId();
        
        // Begin -> Subtract
        model.addLink(begin2PortId, subtractAParamId);
        subtract->parameter()[nodes::FieldNames::B] = nodes::VariantParameter(3.0f);
        
        // Subtract -> ComposeVector
        model.addLink(subtractResultPortId, vectorXParamId);
        composeVector->parameter()["y"] = nodes::VariantParameter(0.0f);
        composeVector->parameter()["z"] = nodes::VariantParameter(0.0f);
        
        // ComposeVector -> End
        model.addLink(vectorResultPortId, shapeParamId);
        
        // Set up connections for the disconnected path
        divide->parameter()[nodes::FieldNames::A] = nodes::VariantParameter(10.0f);
        divide->parameter()[nodes::FieldNames::B] = nodes::VariantParameter(2.0f);
        
        // Get port and parameter IDs for the disconnected path
        auto divideResultPortId = divide->getOutputs().at(nodes::FieldNames::Result).getId();
        auto sineAParamId = sine->parameter().at(nodes::FieldNames::A).getId();
        auto sineResultPortId = sine->getOutputs().at(nodes::FieldNames::Result).getId();
        auto cosineAParamId = cosine->parameter().at(nodes::FieldNames::A).getId();
        
        // Divide -> Sine
        model.addLink(divideResultPortId, sineAParamId);
        
        // Sine -> Cosine
        model.addLink(sineResultPortId, cosineAParamId);
        
        // Count initial nodes (should be 9: Begin, End, 4 connected, 3 disconnected)
        int initialNodeCount = 0;
        for (auto it = model.begin(); it != model.end(); ++it) {
            ++initialNodeCount;
        }
        EXPECT_EQ(initialNodeCount, 9);
        
        // Act
        size_t removedCount = model.simplifyModel();
        
        // Assert
        EXPECT_EQ(removedCount, 3); // Three disconnected nodes should be removed
        
        // Count remaining nodes (should be 6: Begin, End, 4 connected)
        int finalNodeCount = 0;
        for (auto it = model.begin(); it != model.end(); ++it) {
            ++finalNodeCount;
        }
        EXPECT_EQ(finalNodeCount, 6);
        
        // Verify that the disconnected nodes were removed
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Division>(model), 0);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Sine>(model), 0);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Cosine>(model), 0);
        
        // Verify that the connected nodes still exist
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Addition>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Multiplication>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Subtraction>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::ComposeVector>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Begin>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::End>(model), 1);
    }

    TEST_F(Model, VisitNodes_DisconnectedBeginAndEnd_AllNodesAreVisited)
    {
        nodes::Model model;
        model.createBeginEnd();
        
        // Custom visitor that counts all node visits via the base NodeBase method
        class NodeCountVisitor : public nodes::Visitor
        {
        public:
            int count = 0;
            void visit(nodes::NodeBase &) override { ++count; }
        } visitor;
        
        model.visitNodes(visitor);
        
        // Should visit both Begin and End nodes, even though they have no connections
        EXPECT_EQ(visitor.count, 2);
        
        // Verify both Begin and End nodes are present
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::Begin>(model), 1);
        EXPECT_EQ(helper::countNumberOfNodesOfType<nodes::End>(model), 1);
    }

}
