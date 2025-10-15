#include "LowerFunctionGradient.h"

#include "Assembly.h"
#include "DerivedNodes.h"
#include "Model.h"

#include <algorithm>
#include <array>
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
        constexpr float kEpsilon = 1e-8f;

        void linkOrThrow(Model & model, Port & source, VariantParameter & target)
        {
            if (!model.addLink(source.getId(), target.getId()))
            {
                throw std::runtime_error("Failed to link ports while lowering FunctionGradient");
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

    LowerFunctionGradient::LowerFunctionGradient(Assembly & assembly, events::SharedLogger logger)
        : LowerFunctionGradient(assembly, std::move(logger), {})
    {
    }

    LowerFunctionGradient::LowerFunctionGradient(Assembly & assembly,
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

    bool LowerFunctionGradient::GradientSignature::operator==(
      GradientSignature const & other) const noexcept
    {
        return referencedFunction == other.referencedFunction &&
               scalarOutput == other.scalarOutput && vectorInput == other.vectorInput;
    }

    std::size_t LowerFunctionGradient::GradientSignatureHash::operator()(
      GradientSignature const & key) const noexcept
    {
        std::size_t seed = std::hash<ResourceId>{}(key.referencedFunction);
        seed ^= std::hash<std::string>{}(key.scalarOutput) + 0x9e3779b97f4a7c15ULL + (seed << 6) +
                (seed >> 2);
        seed ^= std::hash<std::string>{}(key.vectorInput) + 0x9e3779b97f4a7c15ULL + (seed << 6) +
                (seed >> 2);
        return seed;
    }

    ResourceId LowerFunctionGradient::allocateModelId()
    {
        auto & models = m_assembly.getFunctions();
        while (models.contains(m_nextModelId))
        {
            ++m_nextModelId;
        }
        return m_nextModelId++;
    }

    SharedModel LowerFunctionGradient::getLoweredModel(ResourceId id) const
    {
        return m_assembly.findModel(id);
    }

    std::optional<std::string>
    LowerFunctionGradient::validateConfiguration(FunctionGradient const & gradient,
                                                 Model const * referencedModel) const
    {
        if (!referencedModel)
        {
            return fmt::format("Referenced function with id {} not found.",
                               gradient.getFunctionId());
        }

        if (!gradient.hasValidConfiguration())
        {
            return "Configuration incomplete: select function, scalar output, and vector input.";
        }

        auto const & scalarName = gradient.getSelectedScalarOutput();
        auto const & vectorName = gradient.getSelectedVectorInput();
        if (scalarName.empty())
        {
            return "Scalar output not selected.";
        }
        if (vectorName.empty())
        {
            return "Vector input not selected.";
        }

        auto & outputs = const_cast<Model *>(referencedModel)->getOutputs();
        auto const scalarIter = outputs.find(scalarName);
        if (scalarIter == outputs.end())
        {
            return fmt::format("Scalar output '{}' not found in referenced function.", scalarName);
        }
        if (scalarIter->second.getTypeIndex() != ParameterTypeIndex::Float)
        {
            return fmt::format("Scalar output '{}' has incompatible type; expected float.",
                               scalarName);
        }

        auto & inputs = const_cast<Model *>(referencedModel)->getInputs();
        auto const vectorIter = inputs.find(vectorName);
        if (vectorIter == inputs.end())
        {
            return fmt::format("Vector input '{}' not found in referenced function.", vectorName);
        }
        if (vectorIter->second.getTypeIndex() != ParameterTypeIndex::Float3)
        {
            return fmt::format("Vector input '{}' has incompatible type; expected float3.",
                               vectorName);
        }

        return std::nullopt;
    }

    void LowerFunctionGradient::run()
    {
        m_hadErrors = false;
        std::vector<GradientTarget> gradients;
        gradients.reserve(8);

        for (auto & [modelId, model] : m_assembly.getFunctions())
        {
            if (!model)
            {
                continue;
            }

            for (auto & [nodeId, node] : *model)
            {
                if (auto * gradient = dynamic_cast<FunctionGradient *>(node.get()); gradient)
                {
                    gradients.push_back({modelId, nodeId});
                }
            }
        }

        for (auto const target : gradients)
        {
            auto parentModel = m_assembly.findModel(target.modelId);
            if (!parentModel)
            {
                continue;
            }

            auto nodePtr = parentModel->getNode(target.nodeId);
            if (!nodePtr.has_value())
            {
                continue;
            }

            auto * gradient = dynamic_cast<FunctionGradient *>(nodePtr.value());
            if (!gradient)
            {
                continue;
            }

            gradient->resolveFunctionId();
            auto referencedModel = m_assembly.findModel(gradient->getFunctionId());

            auto validationError = validateConfiguration(*gradient, referencedModel.get());
            if (validationError.has_value())
            {
                reportError(fmt::format("Unable to lower FunctionGradient '{}': {}",
                                        gradient->getDisplayName(),
                                        validationError.value()));
                continue;
            }

            auto loweredId = lowerGradient(*gradient, *parentModel, *referencedModel);
            auto loweredModel = getLoweredModel(loweredId);
            replaceGradientWithCall(*gradient, *parentModel, loweredModel);
        }
    }

    bool LowerFunctionGradient::hadErrors() const noexcept
    {
        return m_hadErrors;
    }

    ResourceId LowerFunctionGradient::lowerGradient(FunctionGradient & gradient,
                                                    Model & parentModel,
                                                    Model & referencedModel)
    {
        GradientSignature signature{gradient.getFunctionId(),
                                    gradient.getSelectedScalarOutput(),
                                    gradient.getSelectedVectorInput()};
        return ensureLoweredFunction(signature, gradient, referencedModel);
    }

    ResourceId LowerFunctionGradient::ensureLoweredFunction(GradientSignature const & key,
                                                            FunctionGradient & gradient,
                                                            Model & referencedModel)
    {
        auto cacheIter = m_cache.find(key);
        if (cacheIter != m_cache.end())
        {
            return cacheIter->second;
        }

        auto newId = allocateModelId();
        auto gradientModel = synthesizeGradientModel(key, gradient, referencedModel, newId);
        m_assembly.getFunctions()[newId] = std::move(gradientModel);
        m_cache[key] = newId;
        return newId;
    }

    SharedModel LowerFunctionGradient::synthesizeGradientModel(GradientSignature const & key,
                                                               FunctionGradient & gradient,
                                                               Model & referencedModel,
                                                               ResourceId newId)
    {
        auto model = std::make_shared<Model>();
        model->setResourceId(newId);
        model->createBeginEnd();

        auto sanitized = sanitizeName(fmt::format("gradient_of_{}_{}_{}",
                                                  referencedModel.getModelName(),
                                                  key.scalarOutput,
                                                  key.vectorInput));
        model->setModelName(sanitized);
        model->setDisplayName(sanitized);

        auto * begin = model->getBeginNode();
        auto * end = model->getEndNode();

        auto addArgument = [&](std::string const & name, std::type_index type)
        {
            VariantParameter parameter = createVariantTypeFromTypeIndex(type);
            parameter.marksAsArgument();
            parameter.setInputSourceRequired(false);
            parameter.setParentId(begin->getId());
            model->addArgument(name, parameter);
        };

        auto & referencedInputs = referencedModel.getInputs();
        for (auto & [inputName, port] : referencedInputs)
        {
            addArgument(inputName, port.getTypeIndex());
        }

        auto & mirroredInputs = model->getInputs();
        if (!mirroredInputs.contains(FieldNames::StepSize))
        {
            VariantParameter parameter = VariantParameter(VariantType{gradient.getStepSize()});
            parameter.marksAsArgument();
            parameter.setInputSourceRequired(false);
            parameter.setParentId(begin->getId());
            model->addArgument(FieldNames::StepSize, parameter);
        }

        auto baseIter = mirroredInputs.find(key.vectorInput);
        if (baseIter == mirroredInputs.end())
        {
            throw std::runtime_error("Referenced vector input missing while lowering gradient");
        }
        auto & basePort = baseIter->second;
        auto & stepPort = mirroredInputs.at(FieldNames::StepSize);

        VariantParameter vectorOutput = createVariantTypeFromTypeIndex(ParameterTypeIndex::Float3);
        vectorOutput.setInputSourceRequired(true);
        vectorOutput.setParentId(end->getId());
        end->parameter()[FieldNames::Vector] = vectorOutput;
        model->registerInput(end->parameter()[FieldNames::Vector]);

        auto * zeroScalar = makeScalar(*model, 0.0f, "zero");
        auto * oneScalar = makeScalar(*model, 1.0f, "one");
        auto * minusOneScalar = makeScalar(*model, -1.0f, "neg_one");
        auto * twoScalar = makeScalar(*model, 2.0f, "two");
        auto * epsilonScalar = makeScalar(*model, kEpsilon, "epsilon");

        auto * absStep = model->create<Abs>();
        absStep->setDisplayName("abs_step");
        linkOrThrow(*model, stepPort, absStep->parameter().at(FieldNames::A));

        auto * safeStep = model->create<Max>();
        safeStep->setDisplayName("safe_step");
        linkOrThrow(*model,
                    absStep->getOutputs().at(FieldNames::Result),
                    safeStep->parameter().at(FieldNames::A));
        linkOrThrow(*model,
                    epsilonScalar->getOutputs().at(FieldNames::Value),
                    safeStep->parameter().at(FieldNames::B));

        auto * negStep = model->create<Multiplication>();
        negStep->setDisplayName("neg_step");
        linkOrThrow(*model,
                    safeStep->getOutputs().at(FieldNames::Result),
                    negStep->parameter().at(FieldNames::A));
        linkOrThrow(*model,
                    minusOneScalar->getOutputs().at(FieldNames::Value),
                    negStep->parameter().at(FieldNames::B));

        auto * twoH = model->create<Multiplication>();
        twoH->setDisplayName("two_h");
        linkOrThrow(*model,
                    safeStep->getOutputs().at(FieldNames::Result),
                    twoH->parameter().at(FieldNames::A));
        linkOrThrow(*model,
                    twoScalar->getOutputs().at(FieldNames::Value),
                    twoH->parameter().at(FieldNames::B));

        std::array<std::string, 3> components{FieldNames::X, FieldNames::Y, FieldNames::Z};
        std::array<Division *, 3> gradientComponents{};

        for (std::size_t axis = 0; axis < components.size(); ++axis)
        {
            auto * composePos = model->create<ComposeVector>();
            composePos->setDisplayName(fmt::format("delta_pos_{}", components[axis]));
            auto * composeNeg = model->create<ComposeVector>();
            composeNeg->setDisplayName(fmt::format("delta_neg_{}", components[axis]));

            for (std::size_t component = 0; component < components.size(); ++component)
            {
                auto const & name = components[component];
                if (component == axis)
                {
                    linkOrThrow(*model,
                                safeStep->getOutputs().at(FieldNames::Result),
                                composePos->parameter().at(name));
                    linkOrThrow(*model,
                                negStep->getOutputs().at(FieldNames::Result),
                                composeNeg->parameter().at(name));
                }
                else
                {
                    linkOrThrow(*model,
                                zeroScalar->getOutputs().at(FieldNames::Value),
                                composePos->parameter().at(name));
                    linkOrThrow(*model,
                                zeroScalar->getOutputs().at(FieldNames::Value),
                                composeNeg->parameter().at(name));
                }
            }

            auto * addPos = model->create<Addition>();
            addPos->setDisplayName(fmt::format("pos_offset_{}", components[axis]));
            linkOrThrow(*model, basePort, addPos->parameter().at(FieldNames::A));
            linkOrThrow(*model,
                        composePos->getOutputs().at(FieldNames::Result),
                        addPos->parameter().at(FieldNames::B));

            auto * addNeg = model->create<Addition>();
            addNeg->setDisplayName(fmt::format("neg_offset_{}", components[axis]));
            linkOrThrow(*model, basePort, addNeg->parameter().at(FieldNames::A));
            linkOrThrow(*model,
                        composeNeg->getOutputs().at(FieldNames::Result),
                        addNeg->parameter().at(FieldNames::B));

            auto configureCall = [&](Addition * offset)
            {
                auto * call = model->create<FunctionCall>();
                call->setDisplayName(fmt::format(
                  "sample_{}_{}", components[axis], offset == addPos ? "posit" : "neg"));
                call->setFunctionId(key.referencedFunction);
                call->updateInputsAndOutputs(referencedModel);
                model->registerInputs(*call);
                model->registerOutputs(*call);

                auto & mirrored = model->getInputs();
                for (auto & [paramName, parameter] : call->parameter())
                {
                    auto mirroredIter = mirrored.find(paramName);
                    if (paramName == FieldNames::FunctionId)
                    {
                        parameter.setValue(VariantType{key.referencedFunction});
                        parameter.setInputSourceRequired(false);
                        continue;
                    }
                    if (paramName == key.vectorInput)
                    {
                        linkOrThrow(*model, offset->getOutputs().at(FieldNames::Result), parameter);
                    }
                    else if (mirroredIter != mirrored.end())
                    {
                        linkOrThrow(*model, mirroredIter->second, parameter);
                    }
                    else
                    {
                        auto * referencedBegin = referencedModel.getBeginNode();
                        if (referencedBegin)
                        {
                            auto referencedParamIter = referencedBegin->parameter().find(paramName);
                            if (referencedParamIter != referencedBegin->parameter().end())
                            {
                                parameter.setValue(referencedParamIter->second.getValue());
                                parameter.setInputSourceRequired(false);
                                continue;
                            }
                        }
                        throw std::runtime_error(
                          "Failed to mirror argument while lowering gradient");
                    }
                }

                return call;
            };

            auto * posCall = configureCall(addPos);
            auto * negCall = configureCall(addNeg);

            auto * diff = model->create<Subtraction>();
            diff->setDisplayName(fmt::format("diff_{}", components[axis]));
            linkOrThrow(*model,
                        posCall->getOutputs().at(key.scalarOutput),
                        diff->parameter().at(FieldNames::A));
            linkOrThrow(*model,
                        negCall->getOutputs().at(key.scalarOutput),
                        diff->parameter().at(FieldNames::B));

            auto * div = model->create<Division>();
            div->setDisplayName(fmt::format("gradient_{}", components[axis]));
            linkOrThrow(*model,
                        diff->getOutputs().at(FieldNames::Result),
                        div->parameter().at(FieldNames::A));
            linkOrThrow(*model,
                        twoH->getOutputs().at(FieldNames::Result),
                        div->parameter().at(FieldNames::B));

            gradientComponents[axis] = div;
        }

        auto * composeGradient = model->create<ComposeVector>();
        composeGradient->setDisplayName("compose_gradient");
        for (std::size_t axis = 0; axis < components.size(); ++axis)
        {
            linkOrThrow(*model,
                        gradientComponents[axis]->getOutputs().at(FieldNames::Result),
                        composeGradient->parameter().at(components[axis]));
        }

        auto * lengthNode = model->create<Length>();
        lengthNode->setDisplayName("gradient_length");
        linkOrThrow(*model,
                    composeGradient->getOutputs().at(FieldNames::Result),
                    lengthNode->parameter().at(FieldNames::A));

        auto * safeLength = model->create<Max>();
        safeLength->setDisplayName("safe_length");
        linkOrThrow(*model,
                    lengthNode->getOutputs().at(FieldNames::Result),
                    safeLength->parameter().at(FieldNames::A));
        linkOrThrow(*model,
                    epsilonScalar->getOutputs().at(FieldNames::Value),
                    safeLength->parameter().at(FieldNames::B));

        auto * safeLengthVector = model->create<VectorFromScalar>();
        safeLengthVector->setDisplayName("safe_length_vector");
        linkOrThrow(*model,
                    safeLength->getOutputs().at(FieldNames::Result),
                    safeLengthVector->parameter().at(FieldNames::A));

        auto * normalized = model->create<Division>();
        normalized->setDisplayName("normalized_gradient");
        linkOrThrow(*model,
                    composeGradient->getOutputs().at(FieldNames::Result),
                    normalized->parameter().at(FieldNames::A));
        linkOrThrow(*model,
                    safeLengthVector->getOutputs().at(FieldNames::Result),
                    normalized->parameter().at(FieldNames::B));

        auto * maskSelect = model->create<Select>();
        maskSelect->setDisplayName("length_mask");
        linkOrThrow(*model,
                    lengthNode->getOutputs().at(FieldNames::Result),
                    maskSelect->parameter().at(FieldNames::A));
        linkOrThrow(*model,
                    epsilonScalar->getOutputs().at(FieldNames::Value),
                    maskSelect->parameter().at(FieldNames::B));
        linkOrThrow(*model,
                    oneScalar->getOutputs().at(FieldNames::Value),
                    maskSelect->parameter().at(FieldNames::C));
        linkOrThrow(*model,
                    zeroScalar->getOutputs().at(FieldNames::Value),
                    maskSelect->parameter().at(FieldNames::D));

        auto * maskVector = model->create<VectorFromScalar>();
        maskVector->setDisplayName("mask_vector");
        linkOrThrow(*model,
                    maskSelect->getOutputs().at(FieldNames::Result),
                    maskVector->parameter().at(FieldNames::A));

        auto * finalProduct = model->create<Multiplication>();
        finalProduct->setDisplayName("gradient_output");
        linkOrThrow(*model,
                    normalized->getOutputs().at(FieldNames::Result),
                    finalProduct->parameter().at(FieldNames::A));
        linkOrThrow(*model,
                    maskVector->getOutputs().at(FieldNames::Result),
                    finalProduct->parameter().at(FieldNames::B));

        linkOrThrow(*model,
                    finalProduct->getOutputs().at(FieldNames::Result),
                    end->parameter().at(FieldNames::Vector));

        model->invalidateGraph();
        model->updateGraphAndOrderIfNeeded();
        return model;
    }

    void LowerFunctionGradient::replaceGradientWithCall(FunctionGradient & gradient,
                                                        Model & parentModel,
                                                        SharedModel const & loweredModel)
    {
        auto * call = parentModel.create<FunctionCall>();
        call->setDisplayName(fmt::format("{}_lowered", gradient.getUniqueName()));
        call->setFunctionId(loweredModel->getResourceId());
        call->updateInputsAndOutputs(*loweredModel);
        parentModel.registerInputs(*call);
        parentModel.registerOutputs(*call);

        auto & gradientParams = gradient.parameter();
        for (auto & [name, parameter] : call->parameter())
        {
            if (name == FieldNames::FunctionId)
            {
                parameter.setSource(OptionalSource{});
                parameter.setInputSourceRequired(false);
                parameter.setValue(VariantType{loweredModel->getResourceId()});
                continue;
            }

            auto iter = gradientParams.find(name);
            if (iter != gradientParams.end())
            {
                copyParameter(parentModel, iter->second, parameter);
            }
            else
            {
                parameter.setInputSourceRequired(false);
            }
        }

        call->resolveFunctionId();

        auto & gradientOut = gradient.getOutputs().at(FieldNames::Vector);
        auto & callOut = call->getOutputs().at(FieldNames::Vector);
        callOut.setIsUsed(gradientOut.isUsed());
        rewireConsumers(parentModel, gradientOut, callOut);

        parentModel.remove(gradient.getId());
        parentModel.invalidateGraph();
        parentModel.updateGraphAndOrderIfNeeded();
    }

    void LowerFunctionGradient::rewireConsumers(Model & model, Port & from, Port & to)
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

    void LowerFunctionGradient::copyParameter(Model & model,
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

    std::string LowerFunctionGradient::sanitizeName(std::string value)
    {
        for (auto & ch : value)
        {
            if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_')
            {
                ch = '_';
            }
        }
        return value;
    }

    void LowerFunctionGradient::reportError(std::string const & message)
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
