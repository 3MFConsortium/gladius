#include <gtest/gtest.h>

#include <nodes/Assembly.h>
#include <nodes/DerivedNodes.h>
#include <nodes/LowerFunctionGradient.h>
#include <nodes/Model.h>
#include <nodes/types.h>

#include <variant>
#include <vector>

namespace gladius_tests
{
    namespace
    {
        using gladius::nodes::Assembly;
        using gladius::nodes::ConstantScalar;
        using gladius::nodes::FunctionCall;
        using gladius::nodes::FunctionGradient;
        using gladius::nodes::LowerFunctionGradient;
        using gladius::nodes::Model;
        using gladius::nodes::Port;
        using gladius::nodes::ResourceId;
        using gladius::nodes::VariantParameter;

        constexpr gladius::nodes::ResourceId ReferencedFunctionId = 4242;
        constexpr char const * VectorInputName = "inputVec";
        constexpr char const * ScalarOutputName = "distance";

        void configureReferencedModel(Model & model)
        {
            model.createBeginEnd();
            model.setResourceId(ReferencedFunctionId);
            model.setModelName("ReferencedFunction");

            VariantParameter vectorArgument{
              gladius::nodes::VariantType{gladius::nodes::float3{0.f, 0.f, 0.f}}};
            vectorArgument.marksAsArgument();
            vectorArgument.setInputSourceRequired(false);
            model.addArgument(VectorInputName, vectorArgument);

            VariantParameter scalarArgument{gladius::nodes::VariantType{0.f}};
            scalarArgument.marksAsArgument();
            scalarArgument.setInputSourceRequired(false);
            model.addArgument("temperature", scalarArgument);

            VariantParameter scalarOutput{gladius::nodes::VariantType{0.f}};
            scalarOutput.setInputSourceRequired(true);
            scalarOutput.setConsumedByFunction(false);
            model.addFunctionOutput(ScalarOutputName, scalarOutput);

            auto * constantZero = model.create<ConstantScalar>();
            constantZero->parameter()[gladius::nodes::FieldNames::Value].setValue(
              gladius::nodes::VariantType{0.f});
            constantZero->parameter()[gladius::nodes::FieldNames::Value].setInputSourceRequired(
              false);

            auto & outputs = model.getOutputs();
            auto const scalarOutIter = outputs.find(ScalarOutputName);
            ASSERT_NE(scalarOutIter, outputs.end());
            ASSERT_TRUE(model.addLink(
              constantZero->getOutputs().at(gladius::nodes::FieldNames::Value).getId(),
              scalarOutIter->second.getId()));

            model.invalidateGraph();
            model.updateGraphAndOrderIfNeeded();
        }

        FunctionGradient *
        addGradientNode(Model & mainModel, Model & referencedModel, bool selectVector = true)
        {
            auto * gradient = mainModel.create<FunctionGradient>();
            gradient->setFunctionId(ReferencedFunctionId);
            gradient->updateInputsAndOutputs(referencedModel);
            gradient->setSelectedScalarOutput(ScalarOutputName);
            if (selectVector)
            {
                gradient->setSelectedVectorInput(VectorInputName);
            }
            mainModel.registerInputs(*gradient);
            mainModel.registerOutputs(*gradient);
            gradient->getOutputs().at(gladius::nodes::FieldNames::Vector).setIsUsed(true);
            return gradient;
        }

        void connectToColor(Model & model, Port & source)
        {
            auto * endNode = model.getEndNode();
            ASSERT_NE(endNode, nullptr);
            auto & colorParam = endNode->parameter().at(gladius::nodes::FieldNames::Color);
            auto const linkResult = model.addLink(source.getId(), colorParam.getId());
            ASSERT_TRUE(linkResult);
        }
    } // namespace

    TEST(LowerFunctionGradientTests, LowersGradientIntoFunctionCall)
    {
        Assembly assembly;
        auto mainModel = assembly.assemblyModel();
        mainModel->createBeginEndWithDefaultInAndOuts();

        assembly.addModelIfNotExisting(ReferencedFunctionId);
        auto referencedModel = assembly.findModel(ReferencedFunctionId);
        ASSERT_NE(referencedModel, nullptr);
        configureReferencedModel(*referencedModel);

        auto * gradient = addGradientNode(*mainModel, *referencedModel);
        auto & gradientStepParam = gradient->parameter().at(gladius::nodes::FieldNames::StepSize);
        auto const originalStepValue = gradientStepParam.getValue();
        auto const originalStepSource = gradientStepParam.getConstSource();

        connectToColor(*mainModel, gradient->getOutputs().at(gladius::nodes::FieldNames::Vector));

        assembly.updateInputsAndOutputs();
        auto modelCountBefore = assembly.getFunctions().size();

        LowerFunctionGradient lowering{assembly};
        lowering.run();

        auto modelCountAfter = assembly.getFunctions().size();
        EXPECT_EQ(modelCountBefore + 1, modelCountAfter);

        FunctionCall * loweredCall = nullptr;
        for (auto & [id, node] : *mainModel)
        {
            if (dynamic_cast<FunctionGradient *>(node.get()) != nullptr)
            {
                ADD_FAILURE() << "FunctionGradient node should have been removed";
            }
            if (auto * call = dynamic_cast<FunctionCall *>(node.get()))
            {
                if (call->getFunctionId() != ReferencedFunctionId)
                {
                    loweredCall = call;
                }
            }
        }
        ASSERT_NE(loweredCall, nullptr);
        auto loweredModel = assembly.findModel(loweredCall->getFunctionId());
        ASSERT_NE(loweredModel, nullptr);
        EXPECT_TRUE(loweredModel->getInputs().contains(VectorInputName));
        EXPECT_TRUE(loweredModel->getInputs().contains(gladius::nodes::FieldNames::StepSize));
        EXPECT_TRUE(loweredModel->getOutputs().contains(gladius::nodes::FieldNames::Vector));

        auto const & functionIdParam =
          loweredCall->parameter().at(gladius::nodes::FieldNames::FunctionId);
        EXPECT_FALSE(functionIdParam.getConstSource().has_value());
        auto const & functionIdValue = functionIdParam.getValue();
        ASSERT_TRUE(std::holds_alternative<ResourceId>(functionIdValue));
        EXPECT_EQ(std::get<ResourceId>(functionIdValue), loweredModel->getResourceId());

        auto const & loweredParams = loweredCall->parameter();
        auto stepIter = loweredParams.find(gladius::nodes::FieldNames::StepSize);
        ASSERT_NE(stepIter, loweredParams.end());
        if (originalStepSource.has_value() && originalStepSource->port)
        {
            ASSERT_TRUE(stepIter->second.getConstSource().has_value());
            EXPECT_EQ(stepIter->second.getConstSource()->portId, originalStepSource->portId);
        }
        else
        {
            ASSERT_FALSE(stepIter->second.getConstSource().has_value());
            auto const & loweredValue = stepIter->second.getValue();
            ASSERT_TRUE(std::holds_alternative<float>(loweredValue));
            ASSERT_TRUE(std::holds_alternative<float>(originalStepValue));
            EXPECT_FLOAT_EQ(std::get<float>(loweredValue), std::get<float>(originalStepValue));
        }

        std::size_t functionCallCount = 0;
        for (auto & [nodeId, node] : *loweredModel)
        {
            auto * functionCall = dynamic_cast<FunctionCall *>(node.get());
            if (!functionCall)
            {
                continue;
            }
            ++functionCallCount;
            EXPECT_EQ(functionCall->getFunctionId(), ReferencedFunctionId);
        }
        EXPECT_EQ(functionCallCount, 6u);
    }

    TEST(LowerFunctionGradientTests, ReusesLoweredFunctionForIdenticalGradients)
    {
        Assembly assembly;
        auto mainModel = assembly.assemblyModel();
        mainModel->createBeginEndWithDefaultInAndOuts();

        assembly.addModelIfNotExisting(ReferencedFunctionId);
        auto referencedModel = assembly.findModel(ReferencedFunctionId);
        ASSERT_NE(referencedModel, nullptr);
        configureReferencedModel(*referencedModel);

        auto * gradientA = addGradientNode(*mainModel, *referencedModel);
        auto * gradientB = addGradientNode(*mainModel, *referencedModel);

        auto * combine = mainModel->create<gladius::nodes::Addition>();
        combine->setDisplayName("combine_gradients");
        ASSERT_TRUE(
          mainModel->addLink(gradientA->getOutputs().at(gladius::nodes::FieldNames::Vector).getId(),
                             combine->parameter().at(gladius::nodes::FieldNames::A).getId()));
        ASSERT_TRUE(
          mainModel->addLink(gradientB->getOutputs().at(gladius::nodes::FieldNames::Vector).getId(),
                             combine->parameter().at(gladius::nodes::FieldNames::B).getId()));
        connectToColor(*mainModel, combine->getOutputs().at(gladius::nodes::FieldNames::Result));

        assembly.updateInputsAndOutputs();

        LowerFunctionGradient lowering{assembly};
        lowering.run();

        std::vector<ResourceId> discoveredIds;
        for (auto & [id, node] : *mainModel)
        {
            if (auto * call = dynamic_cast<FunctionCall *>(node.get()))
            {
                if (call->getFunctionId() != ReferencedFunctionId)
                {
                    discoveredIds.push_back(call->getFunctionId());
                }
            }
        }
        ASSERT_EQ(discoveredIds.size(), 2u);
        EXPECT_EQ(discoveredIds.front(), discoveredIds.back());

        std::size_t additionalModels = 0;
        for (auto & [modelId, model] : assembly.getFunctions())
        {
            if (modelId != assembly.getAssemblyModelId() && modelId != ReferencedFunctionId)
            {
                ++additionalModels;
            }
        }
        EXPECT_EQ(additionalModels, 1u);
    }

    TEST(LowerFunctionGradientTests, InvalidGradientIsNotLoweredAndReportsError)
    {
        Assembly assembly;
        auto mainModel = assembly.assemblyModel();
        mainModel->createBeginEndWithDefaultInAndOuts();

        assembly.addModelIfNotExisting(ReferencedFunctionId);
        auto referencedModel = assembly.findModel(ReferencedFunctionId);
        ASSERT_NE(referencedModel, nullptr);
        configureReferencedModel(*referencedModel);

        auto * gradient = addGradientNode(*mainModel, *referencedModel, false);
        connectToColor(*mainModel, gradient->getOutputs().at(gladius::nodes::FieldNames::Vector));

        auto modelCountBefore = assembly.getFunctions().size();
        assembly.updateInputsAndOutputs();

        std::string errorMessage;
        LowerFunctionGradient lowering{
          assembly, {}, [&](std::string const & message) { errorMessage = message; }};
        lowering.run();

        auto modelCountAfter = assembly.getFunctions().size();
        EXPECT_EQ(modelCountBefore, modelCountAfter);

        EXPECT_TRUE(lowering.hadErrors());
        EXPECT_FALSE(errorMessage.empty()) << errorMessage;
        EXPECT_NE(errorMessage.find("Configuration incomplete"), std::string::npos) << errorMessage;

        auto * endNode = mainModel->getEndNode();
        ASSERT_NE(endNode, nullptr);
        auto & colorParam = endNode->parameter().at(gladius::nodes::FieldNames::Color);
        auto const & source = colorParam.getConstSource();
        ASSERT_TRUE(source.has_value());
        ASSERT_NE(source->port, nullptr);
        auto * gradientParent = dynamic_cast<FunctionGradient *>(source->port->getParent());
        ASSERT_NE(gradientParent, nullptr);
        EXPECT_EQ(gradientParent, gradient);
    }
} // namespace gladius_tests
