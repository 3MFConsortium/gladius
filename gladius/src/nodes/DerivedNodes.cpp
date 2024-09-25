
#include "DerivedNodes.h"

#include "MeshResource.h"
#include "Model.h"
#include "Parameter.h"
#include "nodesfwd.h"
#include <filesystem>
#include <limits>
#include <map>
#include <fmt/format.h>

namespace gladius::nodes
{

    TypeRules operatorFunctionRules()
    {
        TypeRule scalar_scalar = {RuleType::Scalar, InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float},
                                               {FieldNames::B, ParameterTypeIndex::Float}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}};

        TypeRule vector_vector = {RuleType::Vector, InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3},
                                               {FieldNames::B, ParameterTypeIndex::Float3}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}};

        TypeRule matrix_matrix = {RuleType::Matrix, InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4},
                                               {FieldNames::B, ParameterTypeIndex::Matrix4}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Matrix4}}};

        return {scalar_scalar,
                vector_vector,
                matrix_matrix,};
    }

    TypeRules functionRules()
    {
        TypeRule scalar = {RuleType::Scalar, InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float}},
                           OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}};

        TypeRule vector = {RuleType::Vector, InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3}},
                           OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float3}}};

        TypeRule matrix = {RuleType::Matrix,
                           InputTypeMap{{FieldNames::A, ParameterTypeIndex::Matrix4}},
                           OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Matrix4}}};

        return {scalar, vector, matrix};
    }

    TypeRules twoParameterFuncRules()
    {
        TypeRule scalar_scalar = {RuleType::Scalar, InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float},
                                               {FieldNames::B, ParameterTypeIndex::Float}},
                                  OutputTypeMap{{FieldNames::Result, ParameterTypeIndex::Float}}};

        TypeRule vector_vector = {RuleType::Vector, InputTypeMap{{FieldNames::A, ParameterTypeIndex::Float3},
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
        TypeRule rule = {RuleType::Default, InputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3},
                                      {FieldNames::Mesh, ParameterTypeIndex::ResourceId}
                                      },
                         OutputTypeMap{{FieldNames::Distance, ParameterTypeIndex::Float}
                                      }};

        m_typeRules = {rule};
        applyTypeRule(rule);

        m_parameter[FieldNames::Start] = VariantParameter(int{0});
        m_parameter[FieldNames::End] = VariantParameter(int{0});

        m_parameter[FieldNames::Start].hide();
        m_parameter[FieldNames::End].hide();

        updateNodeIds();
    }

    void SignedDistanceToMesh::updateMemoryOffsets(GeneratorContext & generatorContext)
    {
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
                auto & res = resMan.getResource(ResourceKey{*resId});
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
        TypeRule rule = {RuleType::Default, InputTypeMap{{FieldNames::Pos, ParameterTypeIndex::Float3},
                                      {FieldNames::Mesh, ParameterTypeIndex::ResourceId}},
                         OutputTypeMap{{FieldNames::Distance, ParameterTypeIndex::Float}}};

        m_typeRules = {rule};
        applyTypeRule(rule);

        m_parameter[FieldNames::Start] = VariantParameter(int{0});
        m_parameter[FieldNames::End] = VariantParameter(int{0});

        m_parameter[FieldNames::Start].hide();
        m_parameter[FieldNames::End].hide();

        // m_parameter[FieldNames::Min].hide();
        // m_parameter[FieldNames::Max].hide();

        updateNodeIds();
    }

    void UnsignedDistanceToMesh::updateMemoryOffsets(GeneratorContext & generatorContext)
    {
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
                auto & res = resMan.getResource(ResourceKey{*resId});
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
} // namespace gladius::nodes
