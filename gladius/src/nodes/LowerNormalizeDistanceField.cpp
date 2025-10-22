#include "LowerNormalizeDistanceField.h"

#include "Assembly.h"
#include "DerivedNodes.h"
#include "Model.h"

#include <algorithm>
#include <cctype>
#include <fmt/format.h>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace gladius::nodes
{
    namespace
    {
        void linkOrThrow(Model & model, Port & source, VariantParameter & target)
        {
            if (!model.addLink(source.getId(), target.getId()))
            {
                throw std::runtime_error(
                  "Failed to link ports while lowering NormalizeDistanceField");
            }
        }

        ConstantScalar * makeScalar(Model & model, float value, std::string_view name)
        {
            auto * node = model.create<ConstantScalar>();
            node->setDisplayName(std::string{name});
            node->parameter()[FieldNames::Value].setValue(VariantType{value});
            node->parameter()[FieldNames::Value].setInputSourceRequired(false);
            node->parameter()[FieldNames::Value].setModifiable(false);
            return node;
        }
    } // namespace

    LowerNormalizeDistanceField::LowerNormalizeDistanceField(Assembly & assembly,
                                                             events::SharedLogger logger)
        : LowerNormalizeDistanceField(assembly, std::move(logger), {})
    {
    }

    LowerNormalizeDistanceField::LowerNormalizeDistanceField(Assembly & assembly,
                                                             events::SharedLogger logger,
                                                             ErrorReporter reporter)
        : m_assembly(assembly)
        , m_logger(std::move(logger))
        , m_errorReporter(std::move(reporter))
    {
        auto const & models = m_assembly.getFunctions();
        if (!models.empty())
        {
            m_nextModelId = std::max<ResourceId>(models.rbegin()->first + 1u, 1u);
        }
        else
        {
            m_nextModelId = 1u;
        }
    }

    bool LowerNormalizeDistanceField::hadErrors() const noexcept
    {
        return m_hadErrors;
    }

    void LowerNormalizeDistanceField::run()
    {
        std::vector<NormalizeTarget> normalizeNodes;

        for (auto & [modelId, model] : m_assembly.getFunctions())
        {
            if (model->isManaged())
            {
                continue;
            }

            for (auto & [nodeId, node] : *model)
            {
                if (dynamic_cast<NormalizeDistanceField *>(node.get()))
                {
                    normalizeNodes.push_back({modelId, nodeId});
                }
            }
        }

        for (auto const & target : normalizeNodes)
        {
            auto model = m_assembly.findModel(target.modelId);
            if (!model)
            {
                reportError(fmt::format("Model {} not found", target.modelId));
                continue;
            }

            auto nodeOpt = model->getNode(target.nodeId);
            if (!nodeOpt.has_value())
            {
                reportError(
                  fmt::format("Node {} not found in model {}", target.nodeId, target.modelId));
                continue;
            }

            auto * normalizeNode = dynamic_cast<NormalizeDistanceField *>(nodeOpt.value());
            if (!normalizeNode)
            {
                reportError("Node is not a NormalizeDistanceField");
                continue;
            }

            try
            {
                lowerNormalizeNode(*normalizeNode, *model);
            }
            catch (std::exception const & e)
            {
                reportError(fmt::format("Failed to lower NormalizeDistanceField in model {}: {}",
                                        target.modelId,
                                        e.what()));
            }
        }
    }

    ResourceId LowerNormalizeDistanceField::allocateModelId()
    {
        auto & models = m_assembly.getFunctions();
        while (models.contains(m_nextModelId))
        {
            ++m_nextModelId;
        }
        return m_nextModelId++;
    }

    void LowerNormalizeDistanceField::lowerNormalizeNode(NormalizeDistanceField & normalizeNode,
                                                         Model & parentModel)
    {
        // Extract or wrap the distance source into a function
        auto helperFunctionId = extractOrWrapDistanceSource(normalizeNode, parentModel);
        if (!helperFunctionId.has_value())
        {
            reportError("Failed to extract or wrap distance source");
            return;
        }

        // Replace the normalize node with composed graph
        replaceNormalizeWithComposition(normalizeNode, parentModel, *helperFunctionId);
    }

    std::optional<ResourceId>
    LowerNormalizeDistanceField::extractOrWrapDistanceSource(NormalizeDistanceField & normalizeNode,
                                                             Model & parentModel)
    {
        // Create a helper function that mirrors the inputs of the referenced function.
        auto newId = allocateModelId();
        auto helperFunction = createHelperFunction(normalizeNode, parentModel, newId);

        if (!helperFunction)
        {
            return std::nullopt;
        }

        m_assembly.getFunctions()[newId] = helperFunction;
        return newId;
    }

    SharedModel
    LowerNormalizeDistanceField::createHelperFunction(NormalizeDistanceField & normalizeNode,
                                                      Model & parentModel,
                                                      ResourceId newId)
    {
        auto model = std::make_shared<Model>();
        model->setResourceId(newId);
        model->setManaged(true);
        model->setModelName(fmt::format("normalize_distance_helper_{}", newId));
        model->createBeginEnd();

        auto * begin = model->getBeginNode();
        auto * end = model->getEndNode();

        // Resolve referenced function from node
        auto const funcId = normalizeNode.getFunctionId();
        auto referencedModel = m_assembly.findModel(funcId);
        if (!referencedModel)
        {
            // No function found; create placeholder output
            VariantParameter distanceOutput =
              createVariantTypeFromTypeIndex(ParameterTypeIndex::Float);
            distanceOutput.setInputSourceRequired(true);
            distanceOutput.setParentId(end->getId());
            end->parameter()[FieldNames::Distance] = distanceOutput;
            model->registerInput(end->parameter()[FieldNames::Distance]);

            auto * constantZero = makeScalar(*model, 0.0f, "distance_placeholder");
            linkOrThrow(*model,
                        constantZero->getOutputs().at(FieldNames::Value),
                        end->parameter().at(FieldNames::Distance));

            model->invalidateGraph();
            model->updateGraphAndOrderIfNeeded();
            return model;
        }

        // Mirror inputs of referenced function as arguments on helper
        for (auto const & kv : referencedModel->getInputs())
        {
            auto const & name = kv.first;
            auto const & param = kv.second;
            VariantParameter arg = createVariantTypeFromTypeIndex(param.getTypeIndex());
            arg.marksAsArgument();
            arg.setInputSourceRequired(false);
            arg.setParentId(begin->getId());
            model->addArgument(name, arg);
        }

        // Create an inner FunctionCall to the referenced function
        auto * innerCall = model->create<FunctionCall>();
        innerCall->setDisplayName("ndf_inner_call");
        innerCall->setFunctionId(funcId);
        innerCall->updateInputsAndOutputs(*referencedModel);
        model->registerInputs(*innerCall);
        model->registerOutputs(*innerCall);

        // Wire helper Begin arguments to inner FunctionCall parameters by name
        for (auto & kv : innerCall->parameter())
        {
            auto const & argName = kv.first;
            auto & innerParam = kv.second;
            if (!innerParam.isArgument())
            {
                continue;
            }
            auto const & beginOuts = begin->getOutputs();
            auto itOut = beginOuts.find(argName);
            if (itOut != beginOuts.end())
            {
                linkOrThrow(*model, const_cast<Port &>(itOut->second), innerParam);
            }
        }

        // Add End.Distance output and connect it to the chosen scalar from inner call
        VariantParameter distanceOutput = createVariantTypeFromTypeIndex(ParameterTypeIndex::Float);
        distanceOutput.setInputSourceRequired(true);
        distanceOutput.setParentId(end->getId());
        end->parameter()[FieldNames::Distance] = distanceOutput;
        model->registerInput(end->parameter()[FieldNames::Distance]);

        auto const & callOuts = innerCall->getOutputs();
        std::string selectedScalar = normalizeNode.getSelectedScalarOutput();
        Port const * chosenOut = nullptr;
        if (!selectedScalar.empty())
        {
            auto it = callOuts.find(selectedScalar);
            if (it != callOuts.end() && it->second.getTypeIndex() == ParameterTypeIndex::Float)
            {
                chosenOut = &it->second;
            }
        }
        if (!chosenOut)
        {
            auto itDist = callOuts.find(FieldNames::Distance);
            if (itDist != callOuts.end())
            {
                chosenOut = &itDist->second;
            }
        }
        if (!chosenOut)
        {
            for (auto const & kv2 : callOuts)
            {
                auto const & port = kv2.second;
                if (port.getTypeIndex() == ParameterTypeIndex::Float)
                {
                    chosenOut = &port;
                    break;
                }
            }
        }
        if (chosenOut)
        {
            linkOrThrow(
              *model, const_cast<Port &>(*chosenOut), end->parameter().at(FieldNames::Distance));
        }
        else
        {
            auto * constantZero = makeScalar(*model, 0.0f, "distance_placeholder");
            linkOrThrow(*model,
                        constantZero->getOutputs().at(FieldNames::Value),
                        end->parameter().at(FieldNames::Distance));
        }

        model->invalidateGraph();
        model->updateGraphAndOrderIfNeeded();
        return model;
    }

    void LowerNormalizeDistanceField::replaceNormalizeWithComposition(
      NormalizeDistanceField & normalizeNode,
      Model & parentModel,
      ResourceId helperFunctionId)
    {
        // Create a FunctionCall to the helper function to compute the selected scalar (numerator)
        auto * helperCall = parentModel.create<FunctionCall>();
        helperCall->setDisplayName(fmt::format("{}_call", normalizeNode.getUniqueName()));
        helperCall->setFunctionId(helperFunctionId);
        if (auto h = m_assembly.findModel(helperFunctionId))
        {
            helperCall->updateInputsAndOutputs(*h);
        }
        parentModel.registerInputs(*helperCall);
        parentModel.registerOutputs(*helperCall);

        // Create FunctionGradient node (denominator from magnitude)
        auto * gradientNode = parentModel.create<FunctionGradient>();
        gradientNode->setDisplayName(fmt::format("{}_gradient", normalizeNode.getUniqueName()));
        gradientNode->setFunctionId(helperFunctionId);

        auto helperFunction = m_assembly.findModel(helperFunctionId);
        if (helperFunction)
        {
            gradientNode->updateInputsAndOutputs(*helperFunction);
            // Gradient differentiates helper's Distance w.r.t. selected vector input
            gradientNode->setSelectedScalarOutput(FieldNames::Distance);
            std::string selectedVector = normalizeNode.getSelectedVectorInput();
            if (selectedVector.empty())
            {
                // Fallback: prefer Pos or unique Float3
                selectedVector = FieldNames::Pos;
                if (!helperFunction->getInputs().contains(FieldNames::Pos))
                {
                    int countFloat3 = 0;
                    std::string lastName;
                    for (auto const & kv3 : helperFunction->getInputs())
                    {
                        if (kv3.second.getTypeIndex() == ParameterTypeIndex::Float3)
                        {
                            ++countFloat3;
                            lastName = kv3.first;
                        }
                    }
                    if (countFloat3 == 1)
                    {
                        selectedVector = lastName;
                    }
                }
            }
            gradientNode->setSelectedVectorInput(selectedVector);
        }

        parentModel.registerInputs(*gradientNode);
        parentModel.registerOutputs(*gradientNode);

        // Forward mirrored arguments from Normalize node to helperCall and gradient
        for (auto & [pname, gparam] : gradientNode->parameter())
        {
            if (!gparam.isArgument())
                continue;
            auto it = normalizeNode.parameter().find(pname);
            if (it != normalizeNode.parameter().end())
            {
                copyParameter(parentModel, it->second, gparam);
            }
        }
        for (auto & [pname, hparam] : helperCall->parameter())
        {
            if (!hparam.isArgument())
                continue;
            auto it = normalizeNode.parameter().find(pname);
            if (it != normalizeNode.parameter().end())
            {
                copyParameter(parentModel, it->second, hparam);
            }
        }

        // Copy StepSize parameter to gradient
        if (gradientNode->parameter().contains(FieldNames::StepSize))
        {
            auto & stepParam = normalizeNode.parameter().at(FieldNames::StepSize);
            auto & gradientStepParam = gradientNode->parameter().at(FieldNames::StepSize);
            copyParameter(parentModel, stepParam, gradientStepParam);
        }

        // Create Max node for safe magnitude (clamp to epsilon = 1e-8 per spec)
        auto * maxNode = parentModel.create<Max>();
        maxNode->setDisplayName(fmt::format("{}_safe_magnitude", normalizeNode.getUniqueName()));
        linkOrThrow(parentModel,
                    gradientNode->getOutputs().at(FieldNames::Magnitude),
                    maxNode->parameter().at(FieldNames::A));

        // Epsilon is hardcoded to 1e-8 per XSD specification
        auto * epsilonConstant = makeScalar(parentModel, 1e-8f, "epsilon_1e-8");
        linkOrThrow(parentModel,
                    epsilonConstant->getOutputs().at(FieldNames::Value),
                    maxNode->parameter().at(FieldNames::B));

        // Create Division node for normalization: numerator = helperCall.Distance
        auto * divNode = parentModel.create<Division>();
        divNode->setDisplayName(fmt::format("{}_normalized", normalizeNode.getUniqueName()));
        linkOrThrow(parentModel,
                    const_cast<Port &>(helperCall->getOutputs().at(FieldNames::Distance)),
                    divNode->parameter().at(FieldNames::A));

        linkOrThrow(parentModel,
                    maxNode->getOutputs().at(FieldNames::Result),
                    divNode->parameter().at(FieldNames::B));

        // Rewire consumers to the normalized output
        // Note: NormalizeDistanceField output is "result" per XSD spec
        auto & normalizeOutput = normalizeNode.getOutputs().at(FieldNames::Result);
        auto & divOutput = divNode->getOutputs().at(FieldNames::Result);
        rewireConsumers(parentModel, normalizeOutput, divOutput);

        // Remove normalize node
        parentModel.remove(normalizeNode.getId());
        parentModel.invalidateGraph();
    }

    void LowerNormalizeDistanceField::rewireConsumers(Model & model, Port & from, Port & to)
    {
        std::vector<ParameterId> targets;
        auto & registry = model.getParameterRegistry();
        targets.reserve(registry.size());
        for (auto const & [paramId, parameter] : registry)
        {
            if (!parameter)
            {
                continue;
            }
            auto & source = parameter->getSource();
            if (source.has_value() && source->port == &from)
            {
                targets.push_back(paramId);
            }
        }

        for (auto const paramId : targets)
        {
            model.removeLink(from.getId(), paramId);
            model.addLink(to.getId(), paramId);
        }
    }

    void LowerNormalizeDistanceField::copyParameter(Model & model,
                                                    VariantParameter const & sourceParam,
                                                    VariantParameter & targetParam)
    {
        targetParam.setInputSourceRequired(sourceParam.isInputSourceRequired());
        targetParam.setModifiable(sourceParam.isModifiable());
        targetParam.setValid(sourceParam.isValid());
        if (sourceParam.isArgument())
        {
            targetParam.marksAsArgument();
        }

        auto const & source = sourceParam.getConstSource();
        if (source.has_value() && source->port)
        {
            model.addLink(source->port->getId(), targetParam.getId());
        }
        else
        {
            targetParam.setValue(sourceParam.getValue());
        }
    }

    std::string LowerNormalizeDistanceField::sanitizeName(std::string value)
    {
        std::replace_if(
          value.begin(),
          value.end(),
          [](char c) { return !std::isalnum(static_cast<unsigned char>(c)); },
          '_');
        return value;
    }

    void LowerNormalizeDistanceField::reportError(std::string const & message)
    {
        m_hadErrors = true;
        if (m_logger)
        {
            m_logger->addEvent(events::Event{message, events::Severity::Error});
        }
        if (m_errorReporter)
        {
            m_errorReporter(message);
        }
    }
} // namespace gladius::nodes
