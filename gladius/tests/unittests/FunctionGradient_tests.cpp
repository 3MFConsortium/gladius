#include <gtest/gtest.h>

#include <nodes/Assembly.h>
#include <nodes/DerivedNodes.h>
#include <nodes/OptimizeOutputs.h>
#include <nodes/Parameter.h>
#include <nodes/ToOCLVisitor.h>
#include <nodes/nodesfwd.h>
#include <nodes/types.h>

#include <sstream>

namespace gladius_tests
{
    namespace
    {
        using gladius::nodes::FieldNames;
        using gladius::nodes::FunctionGradient;
        using gladius::nodes::Model;
        using gladius::nodes::ParameterMap;
        using gladius::nodes::ParameterTypeIndex;
        using gladius::nodes::VariantParameter;
        using gladius::nodes::VariantType;

        constexpr gladius::nodes::ResourceId ReferencedFunctionId = 4242;
        constexpr char const * VectorInputName = "inputVec";
        constexpr char const * ScalarOutputName = "distance";

        void configureReferencedModel(Model & model)
        {
            model.createBeginEnd();
            model.setResourceId(ReferencedFunctionId);

            VariantParameter vectorArgument{VariantType{gladius::nodes::float3{0.f, 0.f, 0.f}}};
            vectorArgument.marksAsArgument();
            model.addArgument(VectorInputName, vectorArgument);

            VariantParameter scalarArgument{VariantType{0.f}};
            model.addArgument("temperature", scalarArgument);

            VariantParameter scalarOutput{VariantType{0.f}};
            scalarOutput.setConsumedByFunction(false);
            scalarOutput.setInputSourceRequired(false);
            model.addFunctionOutput(ScalarOutputName, scalarOutput);
        }

        void setupAssemblyWithGradient(gladius::nodes::Assembly & assembly,
                                       Model *& referencedModel,
                                       FunctionGradient *& gradientNode)
        {
            auto mainModel = assembly.assemblyModel();
            mainModel->createBeginEndWithDefaultInAndOuts();

            assembly.addModelIfNotExisting(ReferencedFunctionId);
            referencedModel = assembly.findModel(ReferencedFunctionId).get();
            ASSERT_NE(referencedModel, nullptr);
            configureReferencedModel(*referencedModel);

            gradientNode = mainModel->create<FunctionGradient>();
            ASSERT_NE(gradientNode, nullptr);
            gradientNode->setFunctionId(ReferencedFunctionId);
            gradientNode->updateInputsAndOutputs(*referencedModel);
            gradientNode->setSelectedScalarOutput(ScalarOutputName);
            gradientNode->setSelectedVectorInput(VectorInputName);
            mainModel->registerInputs(*gradientNode);
            mainModel->registerOutputs(*gradientNode);
            gradientNode->getOutputs().at(FieldNames::Vector).setIsUsed(true);

            auto * endNode = mainModel->getEndNode();
            ASSERT_NE(endNode, nullptr);
            endNode->parameter()[FieldNames::Color].setInputFromPort(
              gradientNode->getOutputs().at(FieldNames::Vector));
        }
    } // namespace

    TEST(FunctionGradientTests, MirrorsReferencedArgumentsIntoGradient)
    {
        Model referencedModel;
        configureReferencedModel(referencedModel);

        FunctionGradient gradientNode;
        gradientNode.setFunctionId(ReferencedFunctionId);
        gradientNode.updateInputsAndOutputs(referencedModel);

        auto & params = gradientNode.parameter();
        ASSERT_TRUE(params.contains(VectorInputName));
        EXPECT_TRUE(params.at(VectorInputName).isArgument());
        EXPECT_EQ(params.at(VectorInputName).getTypeIndex(), ParameterTypeIndex::Float3);
        EXPECT_TRUE(params.contains(gladius::nodes::FieldNames::StepSize));
        EXPECT_FALSE(params.at(gladius::nodes::FieldNames::StepSize).isArgument());

        auto & outputs = gradientNode.getOutputs();
        ASSERT_TRUE(outputs.contains(FieldNames::Vector));
        EXPECT_EQ(outputs.at(FieldNames::Vector).getTypeIndex(), ParameterTypeIndex::Float3);
        EXPECT_EQ(outputs.size(), 1);
    }

    TEST(FunctionGradientTests, ClearsScalarSelectionWhenOutputMissing)
    {
        Model referencedModel;
        configureReferencedModel(referencedModel);

        FunctionGradient gradientNode;
        gradientNode.setFunctionId(ReferencedFunctionId);
        gradientNode.updateInputsAndOutputs(referencedModel);
        gradientNode.setSelectedScalarOutput(ScalarOutputName);
        gradientNode.setSelectedVectorInput(VectorInputName);
        ASSERT_TRUE(gradientNode.hasValidConfiguration());

        referencedModel.getOutputs().erase(ScalarOutputName);
        gradientNode.updateInputsAndOutputs(referencedModel);

        EXPECT_TRUE(gradientNode.getSelectedScalarOutput().empty());
        EXPECT_EQ(gradientNode.getSelectedVectorInput(), VectorInputName);
    }

    TEST(FunctionGradientTests, ClearsVectorSelectionWhenInputMissing)
    {
        Model referencedModel;
        configureReferencedModel(referencedModel);

        FunctionGradient gradientNode;
        gradientNode.setFunctionId(ReferencedFunctionId);
        gradientNode.updateInputsAndOutputs(referencedModel);
        gradientNode.setSelectedScalarOutput(ScalarOutputName);
        gradientNode.setSelectedVectorInput(VectorInputName);
        ASSERT_TRUE(gradientNode.hasValidConfiguration());

        referencedModel.getInputs().erase(VectorInputName);
        gradientNode.updateInputsAndOutputs(referencedModel);

        EXPECT_TRUE(gradientNode.getSelectedVectorInput().empty());
        EXPECT_EQ(gradientNode.getSelectedScalarOutput(), ScalarOutputName);
    }

    TEST(FunctionGradientTests, OptimizeOutputsMarksSelectedScalarConsumed)
    {
        gladius::nodes::Assembly assembly;
        Model * referencedModel = nullptr;
        FunctionGradient * gradientNode = nullptr;
        setupAssemblyWithGradient(assembly, referencedModel, gradientNode);
        ASSERT_NE(gradientNode, nullptr);
        ASSERT_NE(referencedModel, nullptr);

        gladius::nodes::OptimizeOutputs optimizer{&assembly};
        optimizer.optimize();

        auto & outputs = referencedModel->getOutputs();
        auto iter = outputs.find(ScalarOutputName);
        ASSERT_NE(iter, outputs.end());
        EXPECT_TRUE(iter->second.isConsumedByFunction());
    }

    TEST(FunctionGradientTests, ToOclVisitorEmitsCentralDifferenceGradientCode)
    {
        gladius::nodes::Assembly assembly;
        Model * referencedModel = nullptr;
        FunctionGradient * gradientNode = nullptr;
        setupAssemblyWithGradient(assembly, referencedModel, gradientNode);
        ASSERT_NE(gradientNode, nullptr);
        ASSERT_NE(referencedModel, nullptr);

        ASSERT_TRUE(gradientNode->hasValidConfiguration());
        ASSERT_TRUE(gradientNode->getOutputs().contains(FieldNames::Vector));

        gladius::nodes::OptimizeOutputs optimizer{&assembly};
        optimizer.optimize();

        gladius::nodes::ToOclVisitor visitor;
        auto mainModel = assembly.assemblyModel();
        visitor.setAssembly(&assembly);
        visitor.setModel(mainModel.get());

        ASSERT_NO_THROW(mainModel->visitNodes(visitor));

        std::ostringstream stream;
        ASSERT_NO_THROW(visitor.write(stream));
        auto source = stream.str();

        EXPECT_NE(source.find("fmax(fabs"), std::string::npos);
        EXPECT_NE(source.find("FG_gradient_"), std::string::npos);
        EXPECT_NE(source.find("> 1e-8f"), std::string::npos);
        EXPECT_NE(source.find("FG_neg_"), std::string::npos);
    }

    TEST(FunctionGradientTests, ToOclVisitorFallsBackWhenConfigurationIncomplete)
    {
        gladius::nodes::Assembly assembly;
        auto mainModel = assembly.assemblyModel();
        mainModel->createBeginEndWithDefaultInAndOuts();

        assembly.addModelIfNotExisting(ReferencedFunctionId);
        auto referencedModel = assembly.findModel(ReferencedFunctionId).get();
        ASSERT_NE(referencedModel, nullptr);
        configureReferencedModel(*referencedModel);

        auto * gradientNode = mainModel->create<FunctionGradient>();
        ASSERT_NE(gradientNode, nullptr);
        gradientNode->setFunctionId(ReferencedFunctionId);
        gradientNode->updateInputsAndOutputs(*referencedModel);
        mainModel->registerInputs(*gradientNode);
        mainModel->registerOutputs(*gradientNode);

        auto * endNode = mainModel->getEndNode();
        ASSERT_NE(endNode, nullptr);
        endNode->parameter()[FieldNames::Color].setInputFromPort(
          gradientNode->getOutputs().at(FieldNames::Vector));

        gladius::nodes::OptimizeOutputs optimizer{&assembly};
        optimizer.optimize();

        gladius::nodes::ToOclVisitor visitor;
        visitor.setAssembly(&assembly);
        visitor.setModel(mainModel.get());

        EXPECT_NO_THROW(mainModel->visitNodes(visitor));

        std::ostringstream stream;
        visitor.write(stream);
        auto const source = stream.str();

        EXPECT_NE(source.find("fallback"), std::string::npos);
        EXPECT_NE(source.find("(float3)(0.0f)"), std::string::npos);
    }
}
