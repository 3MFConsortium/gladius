#include <nodes/Assembly.h>
#include <nodes/Builder.h>
#include <nodes/Model.h>

#define CL_TARGET_OPENCL_VERSION 120
#include <gtest/gtest.h>

namespace gladius_tests
{

    using namespace gladius;

    class Builder_Test : public ::testing::Test
    {
        void SetUp() override
        {
        }
    };

    TEST_F(Builder_Test,
           AddTransformationToInputCs_ModelWithBeginAndEnd_ReturnsPortIdOfTransformedPos)
    {
        nodes::Model model;
        model.createBeginEndWithDefaultInAndOuts();
        nodes::Builder builder;
        nodes::Matrix4x4 trafo;
        auto const transformedPosId = builder.addTransformationToInputCs(model, trafo);

        EXPECT_EQ(transformedPosId.getId(), 10001);
    }

    TEST_F(Builder_Test, AddResoureRef_EmptyModel_ModelIncludesTransformationAndPart)
    {
        nodes::Model model;
        model.createBeginEndWithDefaultInAndOuts();
        nodes::Builder builder;
        nodes::Matrix4x4 trafo;
        auto port = builder.addTransformationToInputCs(model, trafo);
        builder.addResourceRef(model, ResourceKey{1}, port);

        EXPECT_EQ(model.getSize(), 5); // Begin, End, Transformation, ResourceId and FunctionCall-Node
    }

    TEST_F(Builder_Test, AddComponentRef_EmptyModel_ModelIncludesTransformationAndPart)
    {
        nodes::Model model;
        model.createBeginEndWithDefaultInAndOuts();
        nodes::Builder builder;
        nodes::Matrix4x4 trafo;
        nodes::Model referencedModel;
        referencedModel.createBeginEndWithDefaultInAndOuts();

        builder.addComponentRef(model, referencedModel, trafo);

        EXPECT_EQ(model.getSize(),
                  5); // Begin, End, Transformation, ResourceId and FunctionCall-Node
    }
}