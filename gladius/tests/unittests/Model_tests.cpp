
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

}
