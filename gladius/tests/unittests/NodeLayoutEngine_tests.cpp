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
