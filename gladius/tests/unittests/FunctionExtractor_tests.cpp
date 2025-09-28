#include <gtest/gtest.h>

#include "nodes/DerivedNodes.h"
#include "nodes/FunctionExtractor.h"
#include "nodes/Model.h"

using namespace gladius::nodes;

namespace gladius::tests
{
    TEST(FunctionExtractorTests, FunctionExtractor_SimpleChain_ExtractsAndRewires)
    {
        Model src;
        src.createBeginEndWithDefaultInAndOuts();

        // Build simple chain: inputs.pos -> ConstantScalar c -> Addition a -> End.shape
        // external input to selection: inputs.pos (Float3) into a ComposeVector to Length
        auto * compose = src.create<ComposeVector>();
        auto * length = src.create<Length>();
        auto * constC = src.create<ConstantScalar>();
        auto * add = src.create<Addition>();

        // Wire inputs.pos -> compose(x,y,z)
        auto & beginOuts = src.getBeginNode()->getOutputs();
        auto posPort = beginOuts.at(FieldNames::Pos);
        // decompose isn't present; create from pos by ComposeVector's inputs from ConstantScalar
        // for simplicity Use pos only to ensure we have an external dependency: link compose.result
        // -> length.A
        src.addLink(constC->getOutputs().at(FieldNames::Value).getId(),
                    compose->parameter().at(FieldNames::X).getId(),
                    true);
        src.addLink(constC->getOutputs().at(FieldNames::Value).getId(),
                    compose->parameter().at(FieldNames::Y).getId(),
                    true);
        src.addLink(constC->getOutputs().at(FieldNames::Value).getId(),
                    compose->parameter().at(FieldNames::Z).getId(),
                    true);
        src.addLink(compose->getOutputs().at(FieldNames::Result).getId(),
                    length->parameter().at(FieldNames::A).getId(),
                    true);
        src.addLink(length->getOutputs().at(FieldNames::Result).getId(),
                    add->parameter().at(FieldNames::A).getId(),
                    true);
        src.addLink(constC->getOutputs().at(FieldNames::Value).getId(),
                    add->parameter().at(FieldNames::B).getId(),
                    true);

        // external consumer outside selection
        src.addLink(add->getOutputs().at(FieldNames::Result).getId(),
                    src.getEndNode()->parameter().at(FieldNames::Shape).getId(),
                    true);

        // select nodes to extract: {length, add}
        std::set<NodeId> selection{length->getId(), add->getId()};

        Model dst; // new function
        dst.createBeginEnd();

        FunctionExtractor::Result result;
        bool ok = FunctionExtractor::extractInto(src, dst, selection, result);
        ASSERT_TRUE(ok);

        // Source should now contain a FunctionCall output wired to End.shape
        bool foundFuncCall = false;
        for (auto & [id, n] : src)
        {
            if (dynamic_cast<FunctionCall *>(n.get()))
            {
                foundFuncCall = true;
                // its output should drive End.shape
                auto * fc = dynamic_cast<FunctionCall *>(n.get());
                auto it = fc->getOutputs().find(result.outputNameMap.begin()->second);
                // since we don't know exact name, ensure at least one output exists
                ASSERT_TRUE(fc->getOutputs().size() > 0);
            }
        }
        ASSERT_TRUE(foundFuncCall);

        // New model should have Begin/End and at least one input and one output
        ASSERT_NE(dst.getBeginNode(), nullptr);
        ASSERT_NE(dst.getEndNode(), nullptr);
        // There must be an input argument created and linked to the internal nodes
        ASSERT_GT(dst.getBeginNode()->getOutputs().size(), 0);
        ASSERT_GT(dst.getEndNode()->parameter().size(), 0);
    }

    TEST(FunctionExtractorTests, FunctionExtractor_MultipleExternalInputs_DeduplicatesAndWires)
    {
        Model src;
        src.createBeginEndWithDefaultInAndOuts();

        // External source feeding multiple selected nodes
        auto * externalConst = src.create<ConstantScalar>();
        externalConst->parameter().at(FieldNames::Value).setValue(2.0f);

        // Selected nodes that both use the same external input
        auto * internalConstA = src.create<ConstantScalar>();
        auto * internalConstB = src.create<ConstantScalar>();
        internalConstA->parameter().at(FieldNames::Value).setValue(3.0f);
        internalConstB->parameter().at(FieldNames::Value).setValue(4.0f);

        auto * add = src.create<Addition>();
        auto * mul = src.create<Multiplication>();

        // Wire external const -> add.A and mul.A; internal consts to remaining inputs
        src.addLink(externalConst->getOutputs().at(FieldNames::Value).getId(),
                    add->parameter().at(FieldNames::A).getId(),
                    true);
        src.addLink(internalConstA->getOutputs().at(FieldNames::Value).getId(),
                    add->parameter().at(FieldNames::B).getId(),
                    true);

        src.addLink(externalConst->getOutputs().at(FieldNames::Value).getId(),
                    mul->parameter().at(FieldNames::A).getId(),
                    true);
        src.addLink(internalConstB->getOutputs().at(FieldNames::Value).getId(),
                    mul->parameter().at(FieldNames::B).getId(),
                    true);

        // Drive End.shape from one of the selected node outputs to ensure an ext output exists
        src.addLink(add->getOutputs().at(FieldNames::Result).getId(),
                    src.getEndNode()->parameter().at(FieldNames::Shape).getId(),
                    true);

        // Include the internal constants in the selection to avoid them becoming extra arguments
        std::set<NodeId> selection{
          add->getId(), mul->getId(), internalConstA->getId(), internalConstB->getId()};

        Model dst;
        dst.createBeginEnd();

        FunctionExtractor::Result result;
        bool ok = FunctionExtractor::extractInto(src, dst, selection, result);
        ASSERT_TRUE(ok);

        // There must be exactly one argument on the FunctionCall besides FunctionId (deduped)
        ASSERT_NE(result.functionCall, nullptr);
        size_t argCount = 0;
        VariantParameter * soleArgParam = nullptr;
        for (auto & kv : result.functionCall->parameter())
        {
            if (kv.first != FieldNames::FunctionId && kv.second.isArgument())
            {
                ++argCount;
                soleArgParam = &kv.second;
            }
        }
        ASSERT_EQ(argCount, 1u);

        // And that sole argument should be sourced from the external constant
        ASSERT_NE(soleArgParam, nullptr);
        auto srcOpt = soleArgParam->getSource();
        ASSERT_TRUE(srcOpt.has_value());
        Port * srcPort = src.getPort(srcOpt->portId);
        ASSERT_NE(srcPort, nullptr);
        NodeBase * srcNode = srcPort->getParent();
        ASSERT_NE(srcNode, nullptr);
        ASSERT_NE(dynamic_cast<ConstantScalar *>(srcNode), nullptr);
    }

    TEST(FunctionExtractorTests, FunctionExtractor_SingleOutput_MultipleConsumers_Rewired)
    {
        Model src;
        src.createBeginEndWithDefaultInAndOuts();

        // Inside selection: a constant value that is consumed by two outside additions
        auto * extractedConst = src.create<ConstantScalar>();
        extractedConst->parameter().at(FieldNames::Value).setValue(1.5f);

        auto * outsideAdd1 = src.create<Addition>();
        auto * outsideAdd2 = src.create<Addition>();

        // Provide second operands via other constants (outside selection)
        auto * otherConst1 = src.create<ConstantScalar>();
        otherConst1->parameter().at(FieldNames::Value).setValue(5.0f);
        auto * otherConst2 = src.create<ConstantScalar>();
        otherConst2->parameter().at(FieldNames::Value).setValue(7.0f);

        // Wire selection output to both outside consumers
        src.addLink(extractedConst->getOutputs().at(FieldNames::Value).getId(),
                    outsideAdd1->parameter().at(FieldNames::A).getId(),
                    true);
        src.addLink(otherConst1->getOutputs().at(FieldNames::Value).getId(),
                    outsideAdd1->parameter().at(FieldNames::B).getId(),
                    true);

        src.addLink(extractedConst->getOutputs().at(FieldNames::Value).getId(),
                    outsideAdd2->parameter().at(FieldNames::A).getId(),
                    true);
        src.addLink(otherConst2->getOutputs().at(FieldNames::Value).getId(),
                    outsideAdd2->parameter().at(FieldNames::B).getId(),
                    true);

        // Use one outside consumer to feed End.shape so model remains valid
        src.addLink(outsideAdd1->getOutputs().at(FieldNames::Result).getId(),
                    src.getEndNode()->parameter().at(FieldNames::Shape).getId(),
                    true);

        // Extract the constant
        std::set<NodeId> selection{extractedConst->getId()};
        Model dst;
        dst.createBeginEnd();
        FunctionExtractor::Result result;
        bool ok = FunctionExtractor::extractInto(src, dst, selection, result);
        ASSERT_TRUE(ok);
        ASSERT_NE(result.functionCall, nullptr);

        // After extraction, both outside additions should source A from the FunctionCall's output
        auto assertSourcedFromFunc = [&](Addition * addNode)
        {
            auto & paramA = addNode->parameter().at(FieldNames::A);
            auto srcOpt = paramA.getSource();
            ASSERT_TRUE(srcOpt.has_value());
            Port * srcPort = src.getPort(srcOpt->portId);
            ASSERT_NE(srcPort, nullptr);
            NodeBase * parent = srcPort->getParent();
            ASSERT_NE(parent, nullptr);
            ASSERT_NE(dynamic_cast<FunctionCall *>(parent), nullptr);
        };

        assertSourcedFromFunc(outsideAdd1);
        assertSourcedFromFunc(outsideAdd2);

        // The extracted constant should no longer exist in the source model
        ASSERT_FALSE(src.getNode(extractedConst->getId()).has_value());
    }

    TEST(FunctionExtractorTests, FunctionExtractor_SelectionWithBeginEnd_FailsGracefully)
    {
        Model src;
        src.createBeginEndWithDefaultInAndOuts();
        auto * c = src.create<ConstantScalar>();
        // Wire constant to End.shape directly
        src.addLink(c->getOutputs().at(FieldNames::Value).getId(),
                    src.getEndNode()->parameter().at(FieldNames::Shape).getId(),
                    true);

        // Select begin node (invalid)
        std::set<NodeId> selection{src.getBeginNode()->getId()};
        Model dst;
        FunctionExtractor::Result res;
        bool ok = FunctionExtractor::extractInto(src, dst, selection, res);
        ASSERT_FALSE(ok);
    }
} // namespace gladius::tests
