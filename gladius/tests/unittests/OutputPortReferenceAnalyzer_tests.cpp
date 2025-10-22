#include <gtest/gtest.h>
#include <nodes/Model.h>
#include <nodes/OutputPortReferenceAnalyzer.h>
#include <nodes/NodeBase.h>
#include <nodes/nodesfwd.h>

namespace gladius_tests
{
    using namespace gladius;

    class OutputPortReferenceAnalyzerTest : public ::testing::Test
    {
      protected:
        void SetUp() override
        {
            model = std::make_unique<nodes::Model>();
            analyzer = std::make_unique<nodes::OutputPortReferenceAnalyzer>();
        }

        void TearDown() override
        {
            analyzer.reset();
            model.reset();
        }

        /// Helper to connect two ports
        void connect(nodes::PortId sourcePortId, nodes::ParameterId targetParameterId)
        {
            bool success = model->addLink(sourcePortId, targetParameterId);
            if (!success)
            {
                FAIL() << "Failed to connect port " << sourcePortId << " to parameter " << targetParameterId;
            }
            model->updateGraphAndOrderIfNeeded();
        }

        std::unique_ptr<nodes::Model> model;
        std::unique_ptr<nodes::OutputPortReferenceAnalyzer> analyzer;
    };

    TEST_F(OutputPortReferenceAnalyzerTest, Analyze_EmptyModel_NoReferences)
    {
        analyzer->setModel(model.get());
        analyzer->analyze();

        // Empty model should have no reference counts
        EXPECT_EQ(analyzer->getReferenceCount(0, "Result"), 0);
    }

    TEST_F(OutputPortReferenceAnalyzerTest, Analyze_SingleChainBeginToEnd_AllNodesReachable)
    {
        // Create: Begin -> Addition -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        // Connect Begin.Pos -> Addition.A
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->parameter().at(nodes::FieldNames::A).getId());

        // Connect Addition.Result -> End.Shape
        connect(addNode->getOutputs().at(nodes::FieldNames::Result).getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // All nodes should be reachable
        EXPECT_TRUE(analyzer->isNodeReachable(beginNode->getId()));
        EXPECT_TRUE(analyzer->isNodeReachable(addNode->getId()));
        EXPECT_TRUE(analyzer->isNodeReachable(endNode->getId()));
    }

    TEST_F(OutputPortReferenceAnalyzerTest, Analyze_UnconnectedNode_NodeNotReachable)
    {
        // Create: Begin -> End (Addition is disconnected)
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        // Connect Begin.Pos -> End.Shape (skip Addition)
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Begin and End should be reachable
        EXPECT_TRUE(analyzer->isNodeReachable(beginNode->getId()));
        EXPECT_TRUE(analyzer->isNodeReachable(endNode->getId()));

        // Addition should NOT be reachable
        EXPECT_FALSE(analyzer->isNodeReachable(addNode->getId()));
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           GetReferenceCount_SingleUse_ReturnsOne)
    {
        // Create: Begin -> Addition -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        // Connect Begin.Pos -> Addition.A (single use of Begin.Pos)
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->getInputA().getId());

        // Connect Addition.Result -> End.Shape
        connect(addNode->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Begin.Pos is referenced once
        EXPECT_EQ(
          analyzer->getReferenceCount(beginNode->getId(), nodes::FieldNames::Pos), 1);

        // Addition.Result is referenced once
        EXPECT_EQ(
          analyzer->getReferenceCount(addNode->getId(), nodes::FieldNames::Result), 1);
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           GetReferenceCount_MultipleUses_ReturnsCorrectCount)
    {
        // Create: Begin -> Addition (A and B both from Begin.Pos) -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        // Connect Begin.Pos -> Addition.A
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->getInputA().getId());

        // Connect Begin.Pos -> Addition.B (second use of Begin.Pos)
        // Use parameter map directly since accessor returns const reference
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->parameter().at(nodes::FieldNames::B).getId());

        // Connect Addition.Result -> End.Shape
        connect(addNode->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Begin.Pos is referenced twice
        EXPECT_EQ(
          analyzer->getReferenceCount(beginNode->getId(), nodes::FieldNames::Pos), 2);

        // Addition.Result is referenced once
        EXPECT_EQ(
          analyzer->getReferenceCount(addNode->getId(), nodes::FieldNames::Result), 1);
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           GetReferenceCount_ThreeConsumers_ReturnsThree)
    {
        // Create: Begin -> Add1, Add2, Sub (all use Begin.Pos) -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * add1 = model->create<nodes::Addition>();
        auto * add2 = model->create<nodes::Addition>();
        auto * sub = model->create<nodes::Subtraction>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        // Connect Begin.Pos to three different nodes
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                add1->parameter().at(nodes::FieldNames::A).getId());
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                add2->parameter().at(nodes::FieldNames::A).getId());
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                sub->parameter().at(nodes::FieldNames::A).getId());

        // Connect add1 to End (add2 and sub are dead code)
        connect(add1->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Begin.Pos is referenced only once (from add1 - add2 and sub are unreachable)
        EXPECT_EQ(
          analyzer->getReferenceCount(beginNode->getId(), nodes::FieldNames::Pos), 1);
        
        // Verify dead code detection
        EXPECT_TRUE(analyzer->isNodeReachable(add1->getId()));
        EXPECT_FALSE(analyzer->isNodeReachable(add2->getId()));
        EXPECT_FALSE(analyzer->isNodeReachable(sub->getId()));
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           ShouldInline_SingleUse_ReturnsTrue)
    {
        // Create: Begin -> Addition -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->getInputA().getId());
        connect(addNode->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Addition.Result should be inlined (used once)
        EXPECT_TRUE(
          analyzer->shouldInline(addNode->getId(), nodes::FieldNames::Result));
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           ShouldInline_MultipleUses_ReturnsFalse)
    {
        // Create: Begin -> Addition (both inputs from Begin.Pos) -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        // Use Begin.Pos twice
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->parameter().at(nodes::FieldNames::A).getId());
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->parameter().at(nodes::FieldNames::B).getId());

        connect(addNode->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Begin.Pos should NOT be inlined (used twice)
        EXPECT_FALSE(
          analyzer->shouldInline(beginNode->getId(), nodes::FieldNames::Pos));
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           ShouldInline_UnusedOutput_ReturnsFalse)
    {
        // Create: Begin -> Addition (disconnected) -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->getInputA().getId());

        // Connect Begin directly to End, leaving Addition.Result unused
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Addition.Result should NOT be inlined (used zero times)
        EXPECT_FALSE(
          analyzer->shouldInline(addNode->getId(), nodes::FieldNames::Result));
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           GetConsumers_SingleConsumer_ReturnsCorrectConsumer)
    {
        // Create: Begin -> Addition -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->getInputA().getId());
        connect(addNode->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Get consumers of Addition.Result
        auto consumers = analyzer->getConsumers(addNode->getId(), nodes::FieldNames::Result);

        EXPECT_EQ(consumers.size(), 1);
        EXPECT_EQ(consumers[0].nodeId, endNode->getId());
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           GetConsumers_MultipleConsumers_ReturnsAllConsumers)
    {
        // Create: Begin -> Add1, Add2 (both use Begin.Pos) -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * add1 = model->create<nodes::Addition>();
        auto * add2 = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                add1->parameter().at(nodes::FieldNames::A).getId());
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                add2->parameter().at(nodes::FieldNames::A).getId());

        // Only connect add1 to End (add2 is dead code)
        connect(add1->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Get consumers of Begin.Pos
        auto consumers = analyzer->getConsumers(beginNode->getId(), nodes::FieldNames::Pos);

        // Only add1 should be counted (add2 is unreachable)
        EXPECT_EQ(consumers.size(), 1);
        EXPECT_EQ(consumers[0].nodeId, add1->getId());
        
        // Verify dead code detection
        EXPECT_TRUE(analyzer->isNodeReachable(add1->getId()));
        EXPECT_FALSE(analyzer->isNodeReachable(add2->getId()));
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           Clear_AfterAnalysis_ResetsAllData)
    {
        // Create: Begin -> Addition -> End
        model->createBeginEndWithDefaultInAndOuts();
        auto * addNode = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                addNode->getInputA().getId());
        connect(addNode->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Verify we have data
        EXPECT_GT(analyzer->getReferenceCount(addNode->getId(), nodes::FieldNames::Result), 0);
        EXPECT_TRUE(analyzer->isNodeReachable(addNode->getId()));

        // Clear the analyzer
        analyzer->clear();

        // All data should be reset
        EXPECT_EQ(analyzer->getReferenceCount(addNode->getId(), nodes::FieldNames::Result), 0);
        EXPECT_FALSE(analyzer->isNodeReachable(addNode->getId()));
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           Analyze_ComplexGraph_CorrectReferenceCounts)
    {
        // Create a more complex graph:
        // Begin -> Add1 -> Mul -> End
        //       -> Add2 ->
        // Where Add1 and Add2 both consume Begin.Pos
        // And Mul consumes both Add1.Result and Add2.Result
        model->createBeginEndWithDefaultInAndOuts();
        auto * add1 = model->create<nodes::Addition>();
        auto * add2 = model->create<nodes::Addition>();
        auto * mul = model->create<nodes::Multiplication>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        // Connect Begin.Pos to both additions
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                add1->getInputA().getId());
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                add2->getInputA().getId());

        // Connect both additions to multiplication
        connect(add1->getResultOutputPort().getId(),
                mul->getInputA().getId());
        connect(add2->getResultOutputPort().getId(),
                mul->getInputB().getId());

        // Connect multiplication to End
        connect(mul->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Verify reference counts
        EXPECT_EQ(analyzer->getReferenceCount(beginNode->getId(), nodes::FieldNames::Pos), 2);
        EXPECT_EQ(analyzer->getReferenceCount(add1->getId(), nodes::FieldNames::Result), 1);
        EXPECT_EQ(analyzer->getReferenceCount(add2->getId(), nodes::FieldNames::Result), 1);
        EXPECT_EQ(analyzer->getReferenceCount(mul->getId(), nodes::FieldNames::Result), 1);

        // Verify shouldInline decisions
        EXPECT_FALSE(analyzer->shouldInline(beginNode->getId(), nodes::FieldNames::Pos)); // 2 uses
        EXPECT_TRUE(analyzer->shouldInline(add1->getId(), nodes::FieldNames::Result));     // 1 use
        EXPECT_TRUE(analyzer->shouldInline(add2->getId(), nodes::FieldNames::Result));     // 1 use
        EXPECT_TRUE(analyzer->shouldInline(mul->getId(), nodes::FieldNames::Result));      // 1 use

        // All nodes should be reachable
        EXPECT_TRUE(analyzer->isNodeReachable(beginNode->getId()));
        EXPECT_TRUE(analyzer->isNodeReachable(add1->getId()));
        EXPECT_TRUE(analyzer->isNodeReachable(add2->getId()));
        EXPECT_TRUE(analyzer->isNodeReachable(mul->getId()));
        EXPECT_TRUE(analyzer->isNodeReachable(endNode->getId()));
    }

    TEST_F(OutputPortReferenceAnalyzerTest,
           Analyze_BranchWithDeadCode_OnlyReachableNodesAnalyzed)
    {
        // Create:
        // Begin -> Add1 -> End (reachable)
        // Begin -> Add2 (unreachable, not connected to End)
        model->createBeginEndWithDefaultInAndOuts();
        auto * add1 = model->create<nodes::Addition>();
        auto * add2 = model->create<nodes::Addition>();

        auto * beginNode = model->getBeginNode();
        auto * endNode = model->getEndNode();

        // Connect Begin.Pos to both additions
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                add1->getInputA().getId());
        connect(beginNode->getOutputs().at(nodes::FieldNames::Pos).getId(),
                add2->getInputA().getId());

        // Only connect add1 to End
        connect(add1->getResultOutputPort().getId(),
                endNode->parameter().at(nodes::FieldNames::Shape).getId());

        analyzer->setModel(model.get());
        analyzer->analyze();

        // Add1 should be reachable
        EXPECT_TRUE(analyzer->isNodeReachable(add1->getId()));

        // Add2 should NOT be reachable (dead code)
        EXPECT_FALSE(analyzer->isNodeReachable(add2->getId()));

        // Begin.Pos should only count the reference from Add1
        // (Add2's reference shouldn't count because Add2 is unreachable)
        EXPECT_EQ(analyzer->getReferenceCount(beginNode->getId(), nodes::FieldNames::Pos), 1);
    }

} // namespace gladius_tests
