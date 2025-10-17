#include <gtest/gtest.h>

#include <nodes/Assembly.h>
#include <nodes/DerivedNodes.h>
#include <nodes/LowerNormalizeDistanceField.h>
#include <nodes/Model.h>
#include <nodes/OptimizeOutputs.h>
#include <nodes/ToOCLVisitor.h>
#include <nodes/types.h>

#include <sstream>

namespace gladius_tests
{
    namespace
    {
        using gladius::nodes::Assembly;
        using gladius::nodes::ConstantScalar;
        using gladius::nodes::FieldNames;
        using gladius::nodes::FunctionGradient;
        using gladius::nodes::LowerNormalizeDistanceField;
        using gladius::nodes::Model;
        using gladius::nodes::NormalizeDistanceField;
        using gladius::nodes::ParameterTypeIndex;
        using gladius::nodes::ResourceId;

        constexpr ResourceId DistanceSourceFunctionId = 1001;

        void configureDistanceSourceFunction(Model & model)
        {
            model.createBeginEnd();
            model.setResourceId(DistanceSourceFunctionId);
            model.setModelName("DistanceSourceFunction");

            gladius::nodes::VariantParameter posArgument{
              gladius::nodes::VariantType{gladius::nodes::float3{0.f, 0.f, 0.f}}};
            posArgument.marksAsArgument();
            posArgument.setInputSourceRequired(false);
            model.addArgument(FieldNames::Pos, posArgument);

            gladius::nodes::VariantParameter distanceOutput{gladius::nodes::VariantType{0.f}};
            distanceOutput.setInputSourceRequired(true);
            distanceOutput.setConsumedByFunction(false);
            model.addFunctionOutput(FieldNames::Distance, distanceOutput);

            auto * constantZero = model.create<ConstantScalar>();
            constantZero->parameter()[FieldNames::Value].setValue(gladius::nodes::VariantType{0.f});
            constantZero->parameter()[FieldNames::Value].setInputSourceRequired(false);

            auto & outputs = model.getOutputs();
            auto const distanceOutIter = outputs.find(FieldNames::Distance);
            ASSERT_NE(distanceOutIter, outputs.end());
            ASSERT_TRUE(model.addLink(constantZero->getOutputs().at(FieldNames::Value).getId(),
                                      distanceOutIter->second.getId()));

            model.invalidateGraph();
            model.updateGraphAndOrderIfNeeded();
        }
    } // namespace

    TEST(NormalizeDistanceFieldTests, NodeHasCorrectInputsAndOutputs)
    {
        NormalizeDistanceField normalizeNode;

        // Initially only config parameters exist (FunctionId/StepSize per spec)
        auto & params = normalizeNode.parameter();
        ASSERT_TRUE(params.contains(FieldNames::FunctionId));
        ASSERT_TRUE(params.contains(FieldNames::StepSize));

        EXPECT_EQ(params.at(FieldNames::FunctionId).getTypeIndex(), ParameterTypeIndex::ResourceId);
        EXPECT_EQ(params.at(FieldNames::StepSize).getTypeIndex(), ParameterTypeIndex::Float);

        // Output is Result per XSD spec
        auto & outputs = normalizeNode.getOutputs();
        ASSERT_TRUE(outputs.contains(FieldNames::Result));
        EXPECT_EQ(outputs.at(FieldNames::Result).getTypeIndex(), ParameterTypeIndex::Float);
    }

    TEST(NormalizeDistanceFieldTests, DefaultParameterValues)
    {
        NormalizeDistanceField normalizeNode;

        auto & params = normalizeNode.parameter();

        // Check step size default (per XSD spec)
        auto stepValue = params.at(FieldNames::StepSize).getValue();
        ASSERT_TRUE(std::holds_alternative<float>(stepValue));
        EXPECT_FLOAT_EQ(std::get<float>(stepValue), 1e-3f);
    }

    TEST(NormalizeDistanceFieldTests, LoweringCreatesHelperFunction)
    {
        Assembly assembly;
        auto mainModel = assembly.assemblyModel();
        mainModel->createBeginEndWithDefaultInAndOuts();

        // Create distance source function
        assembly.addModelIfNotExisting(DistanceSourceFunctionId);
        auto distanceSource = assembly.findModel(DistanceSourceFunctionId);
        ASSERT_NE(distanceSource, nullptr);
        configureDistanceSourceFunction(*distanceSource);

        // Create normalize node
        auto * normalizeNode = mainModel->create<NormalizeDistanceField>();
        ASSERT_NE(normalizeNode, nullptr);
        // Point it at the function and mirror inputs
        normalizeNode->setFunctionId(DistanceSourceFunctionId);
        normalizeNode->updateInputsAndOutputs(*distanceSource);
        // Select scalar and vector
        normalizeNode->setSelectedScalarOutput(FieldNames::Distance);
        normalizeNode->setSelectedVectorInput(FieldNames::Pos);

        mainModel->registerInputs(*normalizeNode);
        mainModel->registerOutputs(*normalizeNode);

        auto * endNode = mainModel->getEndNode();
        ASSERT_NE(endNode, nullptr);
        endNode->parameter()[FieldNames::Distance].setInputFromPort(
          normalizeNode->getOutputs().at(FieldNames::Result));

        assembly.updateInputsAndOutputs();
        auto modelCountBefore = assembly.getFunctions().size();

        LowerNormalizeDistanceField lowering{assembly};
        lowering.run();

        auto modelCountAfter = assembly.getFunctions().size();
        EXPECT_GT(modelCountAfter, modelCountBefore);
    }

    TEST(NormalizeDistanceFieldTests, LoweringRemovesNormalizeNode)
    {
        Assembly assembly;
        auto mainModel = assembly.assemblyModel();
        mainModel->createBeginEndWithDefaultInAndOuts();

        assembly.addModelIfNotExisting(DistanceSourceFunctionId);
        auto distanceSource = assembly.findModel(DistanceSourceFunctionId);
        ASSERT_NE(distanceSource, nullptr);
        configureDistanceSourceFunction(*distanceSource);

        auto * normalizeNode = mainModel->create<NormalizeDistanceField>();
        ASSERT_NE(normalizeNode, nullptr);
        normalizeNode->setFunctionId(DistanceSourceFunctionId);
        normalizeNode->updateInputsAndOutputs(*distanceSource);
        normalizeNode->setSelectedScalarOutput(FieldNames::Distance);
        normalizeNode->setSelectedVectorInput(FieldNames::Pos);

        mainModel->registerInputs(*normalizeNode);
        mainModel->registerOutputs(*normalizeNode);

        auto * endNode = mainModel->getEndNode();
        endNode->parameter()[FieldNames::Distance].setInputFromPort(
          normalizeNode->getOutputs().at(FieldNames::Result));

        assembly.updateInputsAndOutputs();

        LowerNormalizeDistanceField lowering{assembly};
        lowering.run();

        // Check that normalize node was removed
        bool foundNormalizeNode = false;
        for (auto & [id, node] : *mainModel)
        {
            if (dynamic_cast<NormalizeDistanceField *>(node.get()) != nullptr)
            {
                foundNormalizeNode = true;
                break;
            }
        }
        EXPECT_FALSE(foundNormalizeNode);
    }

    TEST(NormalizeDistanceFieldTests, LoweringCreatesFunctionGradientNode)
    {
        Assembly assembly;
        auto mainModel = assembly.assemblyModel();
        mainModel->createBeginEndWithDefaultInAndOuts();

        assembly.addModelIfNotExisting(DistanceSourceFunctionId);
        auto distanceSource = assembly.findModel(DistanceSourceFunctionId);
        ASSERT_NE(distanceSource, nullptr);
        configureDistanceSourceFunction(*distanceSource);

        auto * normalizeNode = mainModel->create<NormalizeDistanceField>();
        ASSERT_NE(normalizeNode, nullptr);
        normalizeNode->setFunctionId(DistanceSourceFunctionId);
        normalizeNode->updateInputsAndOutputs(*distanceSource);
        normalizeNode->setSelectedScalarOutput(FieldNames::Distance);
        normalizeNode->setSelectedVectorInput(FieldNames::Pos);

        mainModel->registerInputs(*normalizeNode);
        mainModel->registerOutputs(*normalizeNode);

        auto * endNode = mainModel->getEndNode();
        endNode->parameter()[FieldNames::Distance].setInputFromPort(
          normalizeNode->getOutputs().at(FieldNames::Result));

        assembly.updateInputsAndOutputs();

        LowerNormalizeDistanceField lowering{assembly};
        lowering.run();

        // Check that FunctionGradient node was created
        bool foundGradientNode = false;
        for (auto & [id, node] : *mainModel)
        {
            if (dynamic_cast<FunctionGradient *>(node.get()) != nullptr)
            {
                foundGradientNode = true;
                break;
            }
        }
        EXPECT_TRUE(foundGradientNode);
    }

    TEST(NormalizeDistanceFieldTests, LoweringCreatesDivisionNode)
    {
        Assembly assembly;
        auto mainModel = assembly.assemblyModel();
        mainModel->createBeginEndWithDefaultInAndOuts();

        assembly.addModelIfNotExisting(DistanceSourceFunctionId);
        auto distanceSource = assembly.findModel(DistanceSourceFunctionId);
        ASSERT_NE(distanceSource, nullptr);
        configureDistanceSourceFunction(*distanceSource);

        auto * normalizeNode = mainModel->create<NormalizeDistanceField>();
        ASSERT_NE(normalizeNode, nullptr);
        normalizeNode->setFunctionId(DistanceSourceFunctionId);
        normalizeNode->updateInputsAndOutputs(*distanceSource);
        normalizeNode->setSelectedScalarOutput(FieldNames::Distance);
        normalizeNode->setSelectedVectorInput(FieldNames::Pos);

        mainModel->registerInputs(*normalizeNode);
        mainModel->registerOutputs(*normalizeNode);

        auto * endNode = mainModel->getEndNode();
        ASSERT_NE(endNode, nullptr);
        endNode->parameter()[FieldNames::Distance].setInputFromPort(
          normalizeNode->getOutputs().at(FieldNames::Result));

        assembly.updateInputsAndOutputs();

        LowerNormalizeDistanceField lowering{assembly};
        lowering.run();

        // Check that Division node was created
        bool foundDivisionNode = false;
        for (auto & [id, node] : *mainModel)
        {
            if (dynamic_cast<gladius::nodes::Division *>(node.get()) != nullptr)
            {
                foundDivisionNode = true;
                break;
            }
        }
        EXPECT_TRUE(foundDivisionNode);
    }

    TEST(NormalizeDistanceFieldTests, CategoryIsMath)
    {
        NormalizeDistanceField normalizeNode;
        EXPECT_EQ(normalizeNode.getCategory(), gladius::nodes::Category::Math);
    }

    TEST(NormalizeDistanceFieldTests, HasDescription)
    {
        NormalizeDistanceField normalizeNode;
        auto description = normalizeNode.getDescription();
        EXPECT_FALSE(description.empty());
        EXPECT_NE(description.find("distance"), std::string::npos);
    }
} // namespace gladius_tests
