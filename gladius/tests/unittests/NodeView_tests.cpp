#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../src/ui/NodeView.h"
#include "../../src/nodes/Model.h"
#include "../../src/nodes/NodeBase.h"
#include "../../src/ui/ModelEditor.h"

namespace gladius::ui::tests
{
    /// Test fixture for NodeView tag replacement functionality
    class NodeViewTagReplacementTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            m_model = std::make_shared<nodes::Model>();
            m_nodeView = std::make_unique<NodeView>();
            
            // Create test nodes with tags
            m_node1Id = m_model->generateNodeId();
            m_node2Id = m_model->generateNodeId();
            m_node3Id = m_model->generateNodeId();
            
            auto node1 = std::make_unique<nodes::NodeBase>();
            node1->setId(m_node1Id);
            node1->setTag("test_group");
            
            auto node2 = std::make_unique<nodes::NodeBase>();
            node2->setId(m_node2Id);
            node2->setTag("test_group");
            
            auto node3 = std::make_unique<nodes::NodeBase>();
            node3->setId(m_node3Id);
            node3->setTag("other_group");
            
            m_model->addNode(std::move(node1));
            m_model->addNode(std::move(node2));
            m_model->addNode(std::move(node3));
            
            m_nodeView->setCurrentModel(m_model);
            m_nodeView->updateNodeGroups();
        }

        void TearDown() override
        {
            m_nodeView.reset();
            m_model.reset();
        }

        std::shared_ptr<nodes::Model> m_model;
        std::unique_ptr<NodeView> m_nodeView;
        nodes::NodeId m_node1Id;
        nodes::NodeId m_node2Id;
        nodes::NodeId m_node3Id;
    };

    /// Test successful tag replacement for a group
    TEST_F(NodeViewTagReplacementTest, ReplaceGroupTag_WithValidParameters_ReplacesAllNodesInGroup)
    {
        // Arrange
        std::string const oldTag = "test_group";
        std::string const newTag = "renamed_group";

        // Act
        bool const result = m_nodeView->replaceGroupTag(oldTag, newTag);

        // Assert
        EXPECT_TRUE(result);
        
        // Verify that both nodes in the group have the new tag
        auto node1 = m_model->getNode(m_node1Id);
        auto node2 = m_model->getNode(m_node2Id);
        auto node3 = m_model->getNode(m_node3Id);
        
        ASSERT_TRUE(node1.has_value());
        ASSERT_TRUE(node2.has_value());
        ASSERT_TRUE(node3.has_value());
        
        EXPECT_EQ(node1.value()->getTag(), newTag);
        EXPECT_EQ(node2.value()->getTag(), newTag);
        EXPECT_EQ(node3.value()->getTag(), "other_group"); // Should remain unchanged
        
        // Verify group structure is updated
        EXPECT_FALSE(m_nodeView->hasGroup(oldTag));
        EXPECT_TRUE(m_nodeView->hasGroup(newTag));
    }

    /// Test that replacement fails with empty old tag
    TEST_F(NodeViewTagReplacementTest, ReplaceGroupTag_WithEmptyOldTag_ReturnsFalse)
    {
        // Act
        bool const result = m_nodeView->replaceGroupTag("", "new_tag");

        // Assert
        EXPECT_FALSE(result);
    }

    /// Test that replacement fails with empty new tag
    TEST_F(NodeViewTagReplacementTest, ReplaceGroupTag_WithEmptyNewTag_ReturnsFalse)
    {
        // Act
        bool const result = m_nodeView->replaceGroupTag("test_group", "");

        // Assert
        EXPECT_FALSE(result);
    }

    /// Test that replacement fails when old and new tags are the same
    TEST_F(NodeViewTagReplacementTest, ReplaceGroupTag_WithSameTags_ReturnsFalse)
    {
        // Act
        bool const result = m_nodeView->replaceGroupTag("test_group", "test_group");

        // Assert
        EXPECT_FALSE(result);
    }

    /// Test that replacement fails when old tag doesn't exist
    TEST_F(NodeViewTagReplacementTest, ReplaceGroupTag_WithNonExistentOldTag_ReturnsFalse)
    {
        // Act
        bool const result = m_nodeView->replaceGroupTag("non_existent", "new_tag");

        // Assert
        EXPECT_FALSE(result);
    }

    /// Test that group structure is correctly updated after tag replacement
    TEST_F(NodeViewTagReplacementTest, ReplaceGroupTag_AfterReplacement_UpdatesGroupStructure)
    {
        // Arrange
        std::string const oldTag = "test_group";
        std::string const newTag = "updated_group";
        
        // Verify initial state
        EXPECT_TRUE(m_nodeView->hasGroup(oldTag));
        EXPECT_FALSE(m_nodeView->hasGroup(newTag));

        // Act
        bool const result = m_nodeView->replaceGroupTag(oldTag, newTag);

        // Assert
        EXPECT_TRUE(result);
        EXPECT_FALSE(m_nodeView->hasGroup(oldTag));
        EXPECT_TRUE(m_nodeView->hasGroup(newTag));
    }

} // namespace gladius::ui::tests
