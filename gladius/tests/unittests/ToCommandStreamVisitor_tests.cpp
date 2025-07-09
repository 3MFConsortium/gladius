#include "opencl_test_helper.h"
#include <nodes/Model.h>
#include <nodes/ToCommandStreamVisitor.h>

#include "gtest/gtest.h"

namespace gladius_tests
{
    namespace
    {
        class ToCommandStreamVisitorTest : public ::testing::Test
        {
          protected:
            void SetUp() override
            {
                m_computeContext = std::make_unique<gladius::ComputeContext>();
            }
            std::unique_ptr<gladius::ComputeContext> m_computeContext;
        };

    } // namespace

    TEST_F(ToCommandStreamVisitorTest,
           Visit_ModelWithBeginAndEndNode_CommandStreamFilledWithCorrectCommands)
    {
        SKIP_IF_OPENCL_UNAVAILABLE();

        gladius::nodes::Assembly assembly;
        assembly.assemblyModel()->createBeginEndWithDefaultInAndOuts();
        gladius::CommandBuffer cmds(*m_computeContext);
        gladius::nodes::ToCommandStreamVisitor dynamicOclVisitor(&cmds, &assembly);
        assembly.visitAssemblyNodes(dynamicOclVisitor);

        EXPECT_EQ(cmds.getSize(), 1u); // 1 command for the end node
    }

    TEST_F(
      ToCommandStreamVisitorTest,
      Visit_ModelWithBeginAndEndNodeAndOneNodeInBetween_UnusedCommandsAreNotAddedToCommandStream)
    {
        SKIP_IF_OPENCL_UNAVAILABLE();

        using namespace gladius::nodes;
        gladius::nodes::Assembly assembly;
        auto & model = assembly.assemblyModel();
        model->createBeginEndWithDefaultInAndOuts();
        auto * addition = model->create<Addition>();

        auto inA = addition->getParameter(FieldNames::A);
        inA->setInputFromPort(model->getInputs().at(FieldNames::Pos));

        gladius::CommandBuffer cmds(*m_computeContext);
        gladius::nodes::ToCommandStreamVisitor dynamicOclVisitor(&cmds, &assembly);
        assembly.visitAssemblyNodes(dynamicOclVisitor);

        EXPECT_EQ(cmds.getSize(), 1u); // 1 command for the end node
    }
}