
#include "DerivedNodes.h"

#include "MeshResource.h"
#include "Model.h"
#include "Parameter.h"
#include "nodesfwd.h"
#include <cmath>
#include <filesystem>
#include <fmt/format.h>
#include <limits>
#include <map>
#include <variant>

namespace gladius::nodes
{

    TypeRules operatorFunctionRules()
    {
        TypeRule scalar_scalar = {RuleType::Scalar,
                                  InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float},
                                               {FieldNames::B, ParameterTypeIndex::Float}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}};

        TypeRule vector_vector = {RuleType::Vector,
                                  InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3},
                                               {FieldNames::B, ParameterTypeIndex::Float3}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}};

        TypeRule matrix_matrix = {RuleType::Matrix,
                                  InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4},
                                               {FieldNames::B, ParameterTypeIndex::Matrix4}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Matrix4}}};

        return {
          scalar_scalar,
          vector_vector,
          matrix_matrix,
        };
    }

    TypeRules functionRules()
    {
        TypeRule scalar = {RuleType::Scalar,
                           InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float}},
                           OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}};

        TypeRule vector = {RuleType::Vector,
                           InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3}},
                           OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}};

        TypeRule matrix = {RuleType::Matrix,
                           InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4}},
                           OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Matrix4}}};

        return {scalar, vector, matrix};
    }

    TypeRules twoParameterFuncRules()
    {
        TypeRule scalar_scalar = {RuleType::Scalar,
                                  InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float},
                                               {FieldNames::B, ParameterTypeIndex::Float}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}};

        TypeRule vector_vector = {RuleType::Vector,
                                  InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3},
                                               {FieldNames::B, ParameterTypeIndex::Float3}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}};

        TypeRule matrix_matrix = {RuleType::Matrix,
                                  InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4},
                                               {FieldNames::B, ParameterTypeIndex::Matrix4}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Matrix4}}};

        return {scalar_scalar, vector_vector, matrix_matrix};
    }

    SignedDistanceToMesh::SignedDistanceToMesh()
        : SignedDistanceToMesh({})
    {
    }

    SignedDistanceToMesh::SignedDistanceToMesh(NodeId id)
        : ClonableNode<SignedDistanceToMesh>(NodeName("SignedDistanceToMesh"),
                                             id,
                                             Category::Primitive)
    {
        TypeRule rule = {RuleType::Default,
                         InputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3},
                                      {FieldNames::Mesh, ParameterTypeIndex::ResourceId}},
                         OutputTypeMap{{FieldNames::Distance, ParameterTypeIndex::Float}}};

        m_typeRules = {rule};
        applyTypeRule(rule);

        m_parameter[FieldNames::Start] = VariantParameter(int{0});
        m_parameter[FieldNames::End] = VariantParameter(int{0});

        m_parameter[FieldNames::Start].hide();
        m_parameter[FieldNames::End].hide();

        m_parameter[FieldNames::Start].setInputSourceRequired(false);
        m_parameter[FieldNames::End].setInputSourceRequired(false);

        updateNodeIds();
    }

    void SignedDistanceToMesh::updateMemoryOffsets(GeneratorContext & generatorContext)
    {

        m_parameter[FieldNames::Start].hide();
        m_parameter[FieldNames::End].hide();

        m_parameter[FieldNames::Start].setInputSourceRequired(false);
        m_parameter[FieldNames::End].setInputSourceRequired(false);

        auto & resMan = generatorContext.resourceManager;

        auto meshParameter = m_parameter.at(FieldNames::Mesh);
        auto sourceParameter = meshParameter.getSource();
        if (!sourceParameter.has_value())
        {
            return;
        }

        // if (!m_outputs.at(FieldNames::Distance).isUsed())
        // {
        //     return;
        // }

        auto * sourcePort = sourceParameter.value().port;
        if (!sourcePort)
        {
            throw std::runtime_error("Invalid source port");
        }

        auto sourceNode = sourcePort->getParent();

        if (!sourceNode)
        {
            return;
        }

        nodes::Resource * resNode = dynamic_cast<nodes::Resource *>(sourceNode);
        if (!resNode)
        {
            return;
        }

        auto variantResId = resNode->parameter().at(FieldNames::ResourceId).getValue();

        if (const auto resId = std::get_if<ResourceId>(&variantResId))
        {
            try
            {
                auto & res = resMan.getResource(ResourceKey{*resId, ResourceType::Mesh});
                res.setInUse(true);

                m_parameter[FieldNames::Start].setValue(res.getStartIndex());
                m_parameter[FieldNames::End].setValue(res.getEndIndex());
            }
            catch (...)
            {
                m_parameter[FieldNames::Start].setValue(0);
                m_parameter[FieldNames::End].setValue(0);
            }
        }
        else
        {
            throw std::runtime_error("Invalid resource id");
        }
    }

    SignedDistanceToBeamLattice::SignedDistanceToBeamLattice()
        : SignedDistanceToBeamLattice({})
    {
    }

    SignedDistanceToBeamLattice::SignedDistanceToBeamLattice(NodeId id)
        : ClonableNode<SignedDistanceToBeamLattice>(NodeName("SignedDistanceToBeamLattice"),
                                                    id,
                                                    Category::Primitive)
    {
        TypeRule rule = {RuleType::Default,
                         InputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3},
                                      {FieldNames::BeamLattice, ParameterTypeIndex::ResourceId}},
                         OutputTypeMap{{FieldNames::Distance, ParameterTypeIndex::Float}}};

        m_typeRules = {rule};
        applyTypeRule(rule);

        m_parameter[FieldNames::Start] = VariantParameter(int{0});
        m_parameter[FieldNames::End] = VariantParameter(int{0});

        m_parameter[FieldNames::Start].hide();
        m_parameter[FieldNames::End].hide();

        m_parameter[FieldNames::Start].setInputSourceRequired(false);
        m_parameter[FieldNames::End].setInputSourceRequired(false);

        updateNodeIds();
    }

    void SignedDistanceToBeamLattice::updateMemoryOffsets(GeneratorContext & generatorContext)
    {

        m_parameter[FieldNames::Start].hide();
        m_parameter[FieldNames::End].hide();

        m_parameter[FieldNames::Start].setInputSourceRequired(false);
        m_parameter[FieldNames::End].setInputSourceRequired(false);

        auto & resMan = generatorContext.resourceManager;

        auto beamLatticeParameter = m_parameter.at(FieldNames::BeamLattice);
        auto sourceParameter = beamLatticeParameter.getSource();
        if (!sourceParameter.has_value())
        {
            return;
        }

        auto * sourcePort = sourceParameter.value().port;
        if (!sourcePort)
        {
            throw std::runtime_error("Invalid source port");
        }

        auto sourceNode = sourcePort->getParent();

        if (!sourceNode)
        {
            return;
        }

        nodes::Resource * resNode = dynamic_cast<nodes::Resource *>(sourceNode);
        if (!resNode)
        {
            return;
        }

        auto variantResId = resNode->parameter().at(FieldNames::ResourceId).getValue();

        if (const auto resId = std::get_if<ResourceId>(&variantResId))
        {
            try
            {
                auto & res = resMan.getResource(ResourceKey{*resId, ResourceType::BeamLattice});
                res.setInUse(true);

                m_parameter[FieldNames::Start].setValue(res.getStartIndex());
                m_parameter[FieldNames::End].setValue(res.getEndIndex());
            }
            catch (...)
            {
                m_parameter[FieldNames::Start].setValue(0);
                m_parameter[FieldNames::End].setValue(0);
            }
        }
        else
        {
            throw std::runtime_error("Invalid resource id");
        }
    }

    UnsignedDistanceToMesh::UnsignedDistanceToMesh()
        : UnsignedDistanceToMesh({})
    {
    }

    UnsignedDistanceToMesh::UnsignedDistanceToMesh(NodeId id)
        : ClonableNode<UnsignedDistanceToMesh>(NodeName("UnsignedDistanceToMesh"),
                                               id,
                                               Category::Primitive)
    {
        TypeRule rule = {RuleType::Default,
                         InputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3},
                                      {FieldNames::Mesh, ParameterTypeIndex::ResourceId}},
                         OutputTypeMap{{FieldNames::Distance, ParameterTypeIndex::Float}}};

        m_typeRules = {rule};
        applyTypeRule(rule);

        m_parameter[FieldNames::Start] = VariantParameter(int{0});
        m_parameter[FieldNames::End] = VariantParameter(int{0});

        m_parameter[FieldNames::Start].hide();
        m_parameter[FieldNames::End].hide();

        m_parameter[FieldNames::Start].setInputSourceRequired(false);
        m_parameter[FieldNames::End].setInputSourceRequired(false);

        updateNodeIds();
    }

    void UnsignedDistanceToMesh::updateMemoryOffsets(GeneratorContext & generatorContext)
    {
        m_parameter[FieldNames::Start].hide();
        m_parameter[FieldNames::End].hide();

        m_parameter[FieldNames::Start].setInputSourceRequired(false);
        m_parameter[FieldNames::End].setInputSourceRequired(false);

        auto & resMan = generatorContext.resourceManager;

        auto meshParameter = m_parameter.at(FieldNames::Mesh);
        auto sourceParameter = meshParameter.getSource();
        if (!sourceParameter.has_value())
        {
            return;
        }

        auto * sourcePort = sourceParameter.value().port;
        if (!sourcePort)
        {
            throw std::runtime_error("Invalid source port");
        }
        auto sourceNode = sourcePort->getParent();

        if (!sourceNode)
        {
            return;
        }

        nodes::Resource * resNode = dynamic_cast<nodes::Resource *>(sourceNode);
        if (!resNode)
        {
            return;
        }

        auto variantResId = resNode->parameter().at(FieldNames::ResourceId).getValue();

        if (const auto resId = std::get_if<ResourceId>(&variantResId))
        {
            try
            {
                auto & res = resMan.getResource(ResourceKey{*resId, ResourceType::Mesh});
                res.setInUse(true);

                m_parameter[FieldNames::Start].setValue(res.getStartIndex());
                m_parameter[FieldNames::End].setValue(res.getEndIndex());
            }
            catch (...)
            {
                m_parameter[FieldNames::Start].setValue(0);
                m_parameter[FieldNames::End].setValue(0);
            }
        }
        else
        {
            throw std::runtime_error("Invalid resource id");
        }
    }

    void FunctionCall::updateInputsAndOutputs(Model & referencedModel)
    {
        auto & inputs = referencedModel.getInputs();
        /*
            1. Loop over all inputs
            2. Find the corresponding input in the parameters
            3. If the input is not found, add it, otherwise update the type
        */
        for (auto & [name, input] : inputs)
        {
            auto iter = m_parameter.find(name);
            if (iter == std::end(m_parameter))
            {
                m_parameter[name] = createVariantTypeFromTypeIndex(input.getTypeIndex());
            }
            else
            {
                if (iter->second.getTypeIndex() != input.getTypeIndex())
                {
                    iter->second = createVariantTypeFromTypeIndex(input.getTypeIndex());
                }
            }
            m_parameter[name].marksAsArgument();
            m_parameter[name].setParentId(getId());
        }

        auto & outputs = referencedModel.getOutputs();
        /*
            1. Loop over all outputs
            2. Find the corresponding output in the parameters
            3. If the output is not found, add it, otherwise update the type
        */
        for (auto & [name, output] : outputs)
        {
            auto iter = m_outputs.find(name);
            if (iter == std::end(m_outputs))
            {
                addOutputPort(name, output.getTypeIndex());
            }
            else
            {
                iter->second.setTypeIndex(output.getTypeIndex());
            }
        }
        updateNodeIds();
    }

    FunctionGradient::FunctionGradient(NodeId id)
        : ClonableNode<FunctionGradient>(NodeName("FunctionGradient"), id, Category::Misc)
    {
        initializeBaseParameters();
    }

    void FunctionGradient::initializeBaseParameters()
    {
        TypeRule rule = {RuleType::Default,
                         InputTypeMap{{FieldNames::FunctionId, ParameterTypeIndex::ResourceId},
                                      {FieldNames::StepSize, ParameterTypeIndex::Float}},
                         OutputTypeMap{{FieldNames::Vector, ParameterTypeIndex::Float3},
                                       {FieldNames::Gradient, ParameterTypeIndex::Float3},
                                       {FieldNames::Magnitude, ParameterTypeIndex::Float}}};
        m_typeRules = {rule};
        applyTypeRule(rule);

        auto & functionIdParameter = m_parameter.at(FieldNames::FunctionId);
        functionIdParameter.setInputSourceRequired(false);
        functionIdParameter.setParentId(getId());

        auto & stepSizeParameter = m_parameter.at(FieldNames::StepSize);
        stepSizeParameter.setInputSourceRequired(false);
        stepSizeParameter.setParentId(getId());
        stepSizeParameter.setModifiable(true);
        stepSizeParameter.setValue(VariantType{1e-3f});

        updateInternalOutputs();
    }

    void FunctionGradient::updateInternalOutputs()
    {
        for (auto iter = m_outputs.begin(); iter != m_outputs.end();)
        {
            if (iter->first != FieldNames::Vector && iter->first != FieldNames::Gradient &&
                iter->first != FieldNames::Magnitude)
            {
                iter = m_outputs.erase(iter);
            }
            else
            {
                ++iter;
            }
        }

        if (m_outputs.find(FieldNames::Vector) == m_outputs.end())
        {
            addOutputPort(FieldNames::Vector, ParameterTypeIndex::Float3);
        }

        if (m_outputs.find(FieldNames::Gradient) == m_outputs.end())
        {
            addOutputPort(FieldNames::Gradient, ParameterTypeIndex::Float3);
        }

        if (m_outputs.find(FieldNames::Magnitude) == m_outputs.end())
        {
            addOutputPort(FieldNames::Magnitude, ParameterTypeIndex::Float);
        }

        updateNodeIds();
    }

    void FunctionGradient::resolveFunctionId()
    {
        auto itFunc = m_parameter.find(FieldNames::FunctionId);
        if (itFunc == m_parameter.end())
        {
            return;
        }

        auto & functionIdParameter = itFunc->second;
        auto functionSource = functionIdParameter.getSource();
        if (!functionSource.has_value())
        {
            auto variantResId = functionIdParameter.getValue();
            if (const auto resId = std::get_if<ResourceId>(&variantResId))
            {
                m_functionId = *resId;
            }
            return;
        }

        auto * sourcePort = functionSource.value().port;
        if (!sourcePort)
        {
            return;
        }

        auto sourceNode = sourcePort->getParent();
        if (!sourceNode)
        {
            return;
        }

        auto paramIt = sourceNode->parameter().find(FieldNames::ResourceId);
        if (paramIt == sourceNode->parameter().end())
        {
            return;
        }

        auto variantResId = paramIt->second.getValue();
        if (const auto resId = std::get_if<ResourceId>(&variantResId))
        {
            m_functionId = *resId;
        }
    }

    void FunctionGradient::setFunctionId(ResourceId functionId)
    {
        m_functionId = functionId;
        auto & functionIdParameter = m_parameter.at(FieldNames::FunctionId);
        functionIdParameter.setValue(VariantType{ResourceId(functionId)});
    }

    void FunctionGradient::setSelectedScalarOutput(const std::string & name)
    {
        m_selectedScalarOutputName = name;
    }

    void FunctionGradient::setSelectedVectorInput(const std::string & name)
    {
        m_selectedVectorInputName = name;
    }

    void FunctionGradient::setStepSize(float h)
    {
        auto & stepSizeParameter = m_parameter.at(FieldNames::StepSize);
        float const clamped = std::max(std::abs(h), 1e-8f);
        stepSizeParameter.setValue(VariantType{clamped});
    }

    float FunctionGradient::getStepSize() const
    {
        auto const iter = m_parameter.find(FieldNames::StepSize);
        if (iter == m_parameter.end())
        {
            return 1e-3f;
        }

        auto value = iter->second.getValue();
        if (auto const step = std::get_if<float>(&value))
        {
            return *step;
        }
        return 1e-3f;
    }

    bool FunctionGradient::hasValidConfiguration() const
    {
        return !m_selectedScalarOutputName.empty() && !m_selectedVectorInputName.empty() &&
               m_functionId != 0;
    }

    VariantParameter * FunctionGradient::findArgumentParameter(const std::string & name)
    {
        auto iter = m_parameter.find(name);
        if (iter == m_parameter.end())
        {
            return nullptr;
        }
        if (!iter->second.isArgument())
        {
            return nullptr;
        }
        return &iter->second;
    }

    const VariantParameter * FunctionGradient::findArgumentParameter(const std::string & name) const
    {
        auto iter = m_parameter.find(name);
        if (iter == m_parameter.end())
        {
            return nullptr;
        }
        if (!iter->second.isArgument())
        {
            return nullptr;
        }
        return &iter->second;
    }

    VariantParameter * FunctionGradient::getSelectedVectorParameter()
    {
        return findArgumentParameter(m_selectedVectorInputName);
    }

    const VariantParameter * FunctionGradient::getSelectedVectorParameter() const
    {
        return findArgumentParameter(m_selectedVectorInputName);
    }

    void FunctionGradient::applyMirroredInputs(Model & referencedModel)
    {
        ParameterMap const oldParameters = m_parameter;

        auto getPreservedParameter = [&](std::string const & key,
                                         VariantParameter defaultValue) -> VariantParameter
        {
            auto iter = oldParameters.find(key);
            if (iter != oldParameters.end())
            {
                return iter->second;
            }
            return defaultValue;
        };

        ParameterMap newParameters;
        newParameters[FieldNames::FunctionId] =
          getPreservedParameter(FieldNames::FunctionId, VariantParameter(ResourceId{0}));

        VariantParameter defaultStepSize = VariantParameter(VariantType{1e-3f});
        defaultStepSize.setInputSourceRequired(false);
        newParameters[FieldNames::StepSize] =
          getPreservedParameter(FieldNames::StepSize, defaultStepSize);

        for (auto & [name, input] : referencedModel.getInputs())
        {
            VariantParameter parameter = createVariantTypeFromTypeIndex(input.getTypeIndex());

            auto oldIter = oldParameters.find(name);
            if (oldIter != oldParameters.end())
            {
                auto const & oldParam = oldIter->second;
                if (oldParam.getTypeIndex() == input.getTypeIndex())
                {
                    parameter = oldParam;
                }
                else if (oldParam.getConstSource().has_value())
                {
                    parameter.setSource(oldParam.getConstSource());
                    parameter.setModifiable(oldParam.isModifiable());
                }
            }

            parameter.marksAsArgument();
            parameter.setParentId(getId());
            parameter.setInputSourceRequired(true);
            newParameters[name] = parameter;
        }

        m_parameter = std::move(newParameters);

        auto & functionIdParameter = m_parameter.at(FieldNames::FunctionId);
        functionIdParameter.setParentId(getId());
        functionIdParameter.setInputSourceRequired(false);

        auto & stepSizeParameter = m_parameter.at(FieldNames::StepSize);
        stepSizeParameter.setParentId(getId());
        stepSizeParameter.setInputSourceRequired(false);
        auto stepValue = stepSizeParameter.getValue();
        if (!std::holds_alternative<float>(stepValue))
        {
            stepSizeParameter.setValue(VariantType{1e-3f});
        }

        updateNodeIds();
    }

    void FunctionGradient::validateSelections(Model & referencedModel)
    {
        auto & outputs = referencedModel.getOutputs();
        auto scalarIter = outputs.find(m_selectedScalarOutputName);
        if (scalarIter == outputs.end() ||
            scalarIter->second.getTypeIndex() != ParameterTypeIndex::Float)
        {
            m_selectedScalarOutputName.clear();
        }

        auto * vectorParam = getSelectedVectorParameter();
        if (!vectorParam || vectorParam->getTypeIndex() != ParameterTypeIndex::Float3)
        {
            m_selectedVectorInputName.clear();
        }
    }

    void FunctionGradient::updateInputsAndOutputs(Model & referencedModel)
    {
        applyMirroredInputs(referencedModel);
        validateSelections(referencedModel);
        updateInternalOutputs();
    }
} // namespace gladius::nodes
