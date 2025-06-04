/**
 * @file NodeLayoutEngine_tests.cpp
 * @brief Unit tests for the NodeLayoutEngine class
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ui/NodeLayoutEngine.h"
#include "nodes/Model.h"
#include "nodes/NodeBase.h"
#include "nodes/DerivedNodes.h"
#include "nodes/types.h"

namespace gladius::ui::tests
{
    class NodeLayoutEngineTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            engine = std::make_unique<NodeLayoutEngine>();
            model = std::make_shared<nodes::Model>();
        }

        void TearDown() override
        {
            engine.reset();
            model.reset();
        }

        std::unique_ptr<NodeLayoutEngine> engine;
        std::shared_ptr<nodes::Model> model;
    };

    /**
     * @brief Test that performAutoLayout doesn't crash with empty model
     */
    TEST_F(NodeLayoutEngineTest, PerformAutoLayout_WithEmptyModel_DoesNotCrash)
    {
        NodeLayoutEngine::LayoutConfig config;
        
        EXPECT_NO_THROW(engine->performAutoLayout(*model, config));
    }

    /**
     * @brief Test that performAutoLayout works with single node
     */
    TEST_F(NodeLayoutEngineTest, PerformAutoLayout_WithSingleNode_PositionsNodeCorrectly)
    {
        // Arrange
        auto node = model->create<nodes::ConstantScalar>();
        node->screenPos() = gladius::nodes::float2{0, 0};
        
        NodeLayoutEngine::LayoutConfig config;
        config.nodeDistance = 100.0f;
        
        // Act
        EXPECT_NO_THROW(engine->performAutoLayout(*model, config));
        
        // Assert - Node should be positioned (exact position depends on implementation details)
        auto pos = node->screenPos();
        EXPECT_GE(pos.x, 0.0f);
        EXPECT_GE(pos.y, 0.0f);
    }

    /**
     * @brief Test that performAutoLayout works with multiple nodes
     */
    TEST_F(NodeLayoutEngineTest, PerformAutoLayout_WithMultipleNodes_PositionsNodesWithoutOverlap)
    {
        // Arrange
        auto node1 = model->create<nodes::ConstantScalar>();
        auto node2 = model->create<nodes::ConstantScalar>();
        
        node1->screenPos() = gladius::nodes::float2{0, 0};
        node2->screenPos() = gladius::nodes::float2{0, 0};
        
        NodeLayoutEngine::LayoutConfig config;
        config.nodeDistance = 100.0f;
        
        // Act
        EXPECT_NO_THROW(engine->performAutoLayout(*model, config));
        
        // Assert - Nodes should be positioned differently
        auto pos1 = node1->screenPos();
        auto pos2 = node2->screenPos();
        
        // Nodes should not be at the same position
        EXPECT_TRUE(pos1.x != pos2.x || pos1.y != pos2.y);
    }

    /**
     * @brief Test that layout config parameters affect positioning
     */
    TEST_F(NodeLayoutEngineTest, PerformAutoLayout_WithDifferentConfigs_ProducesDifferentLayouts)
    {
        // Arrange
        auto node1 = model->create<nodes::ConstantScalar>();
        auto node2 = model->create<nodes::ConstantScalar>();
        
        node1->screenPos() = gladius::nodes::float2{0, 0};
        node2->screenPos() = gladius::nodes::float2{0, 0};
        
        // First layout with small distance
        NodeLayoutEngine::LayoutConfig config1;
        config1.nodeDistance = 50.0f;
        
        engine->performAutoLayout(*model, config1);
        auto pos1_small = node1->screenPos();
        auto pos2_small = node2->screenPos();
        
        // Reset positions
        node1->screenPos() = gladius::nodes::float2{0, 0};
        node2->screenPos() = gladius::nodes::float2{0, 0};
        
        // Second layout with large distance
        NodeLayoutEngine::LayoutConfig config2;
        config2.nodeDistance = 200.0f;
        
        engine->performAutoLayout(*model, config2);
        auto pos1_large = node1->screenPos();
        auto pos2_large = node2->screenPos();
        
        // Assert - Larger distance should produce more spread out layout
        float distance_small = std::abs(pos1_small.x - pos2_small.x) + std::abs(pos1_small.y - pos2_small.y);
        float distance_large = std::abs(pos1_large.x - pos2_large.x) + std::abs(pos1_large.y - pos2_large.y);
        
        EXPECT_GT(distance_large, distance_small);
    }

    /**
     * @brief Test that nodes with same tag (group) are positioned together
     */
    TEST_F(NodeLayoutEngineTest, PerformAutoLayout_WithGroupedNodes_KeepsGroupedNodesTogether)
    {
        // Arrange
        auto node1 = model->create<nodes::ConstantScalar>();
        auto node2 = model->create<nodes::ConstantScalar>();
        auto node3 = model->create<nodes::ConstantScalar>();
        
        // Group nodes 1 and 2 together
        node1->setTag("group1");
        node2->setTag("group1");
        node3->setTag("group2");
        
        node1->screenPos() = gladius::nodes::float2{0, 0};
        node2->screenPos() = gladius::nodes::float2{0, 0};
        node3->screenPos() = gladius::nodes::float2{0, 0};
        
        NodeLayoutEngine::LayoutConfig config;
        config.nodeDistance = 100.0f;
        
        // Act
        EXPECT_NO_THROW(engine->performAutoLayout(*model, config));
        
        // Assert - Nodes in same group should be closer to each other than to different group
        auto pos1 = node1->screenPos();
        auto pos2 = node2->screenPos();
        auto pos3 = node3->screenPos();
        
        float distance_within_group = std::sqrt(std::pow(pos1.x - pos2.x, 2) + std::pow(pos1.y - pos2.y, 2));
        float distance_between_groups = std::sqrt(std::pow(pos1.x - pos3.x, 2) + std::pow(pos1.y - pos3.y, 2));
        
        // Nodes within the same group should be closer than nodes in different groups
        EXPECT_LT(distance_within_group, distance_between_groups);
    }

    /**
     * @brief Test edge case with very large nodeDistance
     */
    TEST_F(NodeLayoutEngineTest, PerformAutoLayout_WithLargeNodeDistance_DoesNotOverflow)
    {
        // Arrange
        auto node = model->create<nodes::ConstantScalar>();
        node->screenPos() = gladius::nodes::float2{0, 0};
        
        NodeLayoutEngine::LayoutConfig config;
        config.nodeDistance = 10000.0f;
        
        // Act & Assert
        EXPECT_NO_THROW(engine->performAutoLayout(*model, config));
        
        // Position should be finite
        auto pos = node->screenPos();
        EXPECT_TRUE(std::isfinite(pos.x));
        EXPECT_TRUE(std::isfinite(pos.y));
    }

    /**
     * @brief Test edge case with zero nodeDistance
     */
    TEST_F(NodeLayoutEngineTest, PerformAutoLayout_WithZeroNodeDistance_DoesNotCrash)
    {
        // Arrange
        auto node1 = model->create<nodes::ConstantScalar>();
        auto node2 = model->create<nodes::ConstantScalar>();
        
        node1->screenPos() = gladius::nodes::float2{0, 0};
        node2->screenPos() = gladius::nodes::float2{0, 0};
        
        NodeLayoutEngine::LayoutConfig config;
        config.nodeDistance = 0.0f;
        
        // Act & Assert
        EXPECT_NO_THROW(engine->performAutoLayout(*model, config));
    }

} // namespace gladius::ui::tests
