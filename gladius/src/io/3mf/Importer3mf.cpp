#include "Importer3mf.h"
#include "BeamLatticeImporter.h"

#include <fmt/format.h>
#include <lib3mf_abi.hpp>
#include <lib3mf_implicit.hpp>
#include <lib3mf_types.hpp>
#include <map>
#include <set>
#include <vector>

#include "BeamLatticeResource.h"
#include "Builder.h"
#include "Document.h"
#include "FunctionComparator.h"
#include "ImageExtractor.h"
#include "Parameter.h"
#include "Profiling.h"
#include "VdbImporter.h"
#include "nodes/DerivedNodes.h"
#include "nodes/utils.h"
#include <Eigen/Core>
#include <variant>

namespace gladius
{
    namespace nodes
    {
        class Builder;
    }
}

namespace gladius::io
{

    // Convert 3MF model unit to scaling from millimeters to model units
    // Returns units_per_mm so that: position_in_model_units = position_in_mm * units_per_mm
    static float computeUnitsPerMM(Lib3MF::PModel const & model)
    {
        if (!model)
        {
            return 1.0f;
        }
        Lib3MF::eModelUnit unit = model->GetUnit();
        // mm per unit
        double mm_per_unit = 1.0;
        switch (unit)
        {
        case Lib3MF::eModelUnit::MicroMeter:
            mm_per_unit = 0.001; // 1 µm = 0.001 mm
            break;
        case Lib3MF::eModelUnit::MilliMeter:
            mm_per_unit = 1.0;
            break;
        case Lib3MF::eModelUnit::CentiMeter:
            mm_per_unit = 10.0;
            break;
        case Lib3MF::eModelUnit::Meter:
            mm_per_unit = 1000.0;
            break;
        case Lib3MF::eModelUnit::Inch:
            mm_per_unit = 25.4;
            break;
        case Lib3MF::eModelUnit::Foot:
            mm_per_unit = 304.8;
            break;
        default:
            mm_per_unit = 1.0; // Default/fallback to millimeters
            break;
        }
        // units per mm
        double units_per_mm = 1.0 / mm_per_unit;
        return static_cast<float>(units_per_mm);
    }

    Importer3mf::Importer3mf(events::SharedLogger logger)
        : m_eventLogger(logger)
    {
        ProfileFunction
        try
        {
            m_wrapper = Lib3MF::CWrapper::loadLibrary();
        }
        catch (std::exception & e)
        {
            std::cerr << e.what() << std::endl;
            return;
        }
    }

    void Importer3mf::logWarnings(std::filesystem::path const & filename, Lib3MF::PReader reader)
    {
        if (!m_eventLogger)
        {
            throw std::runtime_error("No event logger set");
        }
        for (Lib3MF_uint32 iWarning = 0; iWarning < reader->GetWarningCount(); iWarning++)
        {
            Lib3MF_uint32 nErrorCode;
            std::string sWarningMessage = reader->GetWarning(iWarning, nErrorCode);

            m_eventLogger->addEvent({fmt::format("Warning #{} while reading 3mf file {}: {}",
                                                 filename.string(),
                                                 nErrorCode,
                                                 sWarningMessage),
                                     events::Severity::Warning});
        }
    }

    gladius::nodes::VariantParameter parameterFromType(Lib3MF::eImplicitPortType type)
    {
        using namespace gladius::nodes;
        switch (type)
        {
        case Lib3MF::eImplicitPortType::Scalar:
            return gladius::nodes::VariantParameter(0.f);
        case Lib3MF::eImplicitPortType::Vector:
            return gladius::nodes::VariantParameter(float3{0.f, 0.f, 0.f});
        case Lib3MF::eImplicitPortType::Matrix:
        {
            return gladius::nodes::VariantParameter(Matrix4x4(), ContentType::Transformation);
        }
        default:
            return gladius::nodes::VariantParameter(0);
        }
    }

    std::type_index typeIndexFrom3mfType(Lib3MF::eImplicitPortType type)
    {
        using namespace gladius::nodes;
        switch (type)
        {
        case Lib3MF::eImplicitPortType::Scalar:
            return ParameterTypeIndex::Float;
        case Lib3MF::eImplicitPortType::Vector:
            return ParameterTypeIndex::Float3;
        case Lib3MF::eImplicitPortType::Matrix:
        {
            return ParameterTypeIndex::Matrix4;
        }
        default:
            return ParameterTypeIndex::Float;
        }
    }

    gladius::nodes::NodeBase * createNode(gladius::nodes::Model & model,
                                          Lib3MF::eImplicitNodeType type)
    {
        ProfileFunction using namespace gladius;
        switch (type)
        {
        case Lib3MF::eImplicitNodeType::Addition:
            return model.create<nodes::Addition>();
        case Lib3MF::eImplicitNodeType::Subtraction:
            return model.create<nodes::Subtraction>();
        case Lib3MF::eImplicitNodeType::Multiplication:
            return model.create<nodes::Multiplication>();
        case Lib3MF::eImplicitNodeType::Division:
            return model.create<nodes::Division>();
        case Lib3MF::eImplicitNodeType::Constant:
            return model.create<nodes::ConstantScalar>();
        case Lib3MF::eImplicitNodeType::ConstVec:
            return model.create<nodes::ConstantVector>();
        case Lib3MF::eImplicitNodeType::ConstMat:
            return model.create<nodes::ConstantMatrix>();
        case Lib3MF::eImplicitNodeType::ComposeVector:
            return model.create<nodes::ComposeVector>();
        case Lib3MF::eImplicitNodeType::DecomposeVector:
            return model.create<nodes::DecomposeVector>();
        case Lib3MF::eImplicitNodeType::ComposeMatrix:
            return model.create<nodes::ComposeMatrix>();
        case Lib3MF::eImplicitNodeType::MatrixFromColumns:
            return model.create<nodes::ComposeMatrixFromColumns>();
        case Lib3MF::eImplicitNodeType::MatrixFromRows:
            return model.create<nodes::ComposeMatrixFromRows>();
        case Lib3MF::eImplicitNodeType::Dot:
            return model.create<nodes::DotProduct>();
        case Lib3MF::eImplicitNodeType::Cross:
            return model.create<nodes::CrossProduct>();
        case Lib3MF::eImplicitNodeType::MatVecMultiplication:
            return model.create<nodes::MatrixVectorMultiplication>();
        case Lib3MF::eImplicitNodeType::Transpose:
            return model.create<nodes::Transpose>();
        case Lib3MF::eImplicitNodeType::Inverse:
            return model.create<nodes::Inverse>();
        case Lib3MF::eImplicitNodeType::Sinus:
            return model.create<nodes::Sine>();
        case Lib3MF::eImplicitNodeType::Cosinus:
            return model.create<nodes::Cosine>();
        case Lib3MF::eImplicitNodeType::Tan:
            return model.create<nodes::Tangent>();
        case Lib3MF::eImplicitNodeType::ArcSin:
            return model.create<nodes::ArcSin>();
        case Lib3MF::eImplicitNodeType::ArcCos:
            return model.create<nodes::ArcCos>();
        case Lib3MF::eImplicitNodeType::ArcTan:
            return model.create<nodes::ArcTan>();
        case Lib3MF::eImplicitNodeType::ArcTan2:
            return model.create<nodes::ArcTan2>();
        case Lib3MF::eImplicitNodeType::Min:
            return model.create<nodes::Min>();
        case Lib3MF::eImplicitNodeType::Max:
            return model.create<nodes::Max>();
        case Lib3MF::eImplicitNodeType::Abs:
            return model.create<nodes::Abs>();
        case Lib3MF::eImplicitNodeType::Fmod:
            return model.create<nodes::Fmod>();
        case Lib3MF::eImplicitNodeType::Pow:
            return model.create<nodes::Pow>();
        case Lib3MF::eImplicitNodeType::Sqrt:
            return model.create<nodes::Sqrt>();
        case Lib3MF::eImplicitNodeType::Exp:
            return model.create<nodes::Exp>();
        case Lib3MF::eImplicitNodeType::Log:
            return model.create<nodes::Log>();
        case Lib3MF::eImplicitNodeType::Log2:
            return model.create<nodes::Log2>();
        case Lib3MF::eImplicitNodeType::Log10:
            return model.create<nodes::Log10>();
        case Lib3MF::eImplicitNodeType::Select:
            return model.create<nodes::Select>();
        case Lib3MF::eImplicitNodeType::Clamp:
            return model.create<nodes::Clamp>();
        case Lib3MF::eImplicitNodeType::Sinh:
            return model.create<nodes::SinH>();
        case Lib3MF::eImplicitNodeType::Cosh:
            return model.create<nodes::CosH>();
        case Lib3MF::eImplicitNodeType::Tanh:
            return model.create<nodes::TanH>();
        case Lib3MF::eImplicitNodeType::Round:
            return model.create<nodes::Round>();
        case Lib3MF::eImplicitNodeType::Ceil:
            return model.create<nodes::Ceil>();
        case Lib3MF::eImplicitNodeType::Floor:
            return model.create<nodes::Floor>();
        case Lib3MF::eImplicitNodeType::Sign:
            return model.create<nodes::Sign>();
        case Lib3MF::eImplicitNodeType::Fract:
            return model.create<nodes::Fract>();
        case Lib3MF::eImplicitNodeType::FunctionCall:
            return model.create<nodes::FunctionCall>();
        case Lib3MF::eImplicitNodeType::Mesh:
            return model.create<nodes::SignedDistanceToMesh>();
        case Lib3MF::eImplicitNodeType::Length:
            return model.create<nodes::Length>();
        case Lib3MF::eImplicitNodeType::ConstResourceID:
            return model.create<nodes::Resource>();
        case Lib3MF::eImplicitNodeType::VectorFromScalar:
            return model.create<nodes::VectorFromScalar>();
        case Lib3MF::eImplicitNodeType::UnsignedMesh:
            return model.create<nodes::UnsignedDistanceToMesh>();
        case Lib3MF::eImplicitNodeType::Mod:
            return model.create<nodes::Mod>();
        default:
            throw std::runtime_error("Unknown node type");
            return nullptr;
        }
    }

    // Extract Nodename from "NodeName.OutputName"
    std::string extractNodeName(std::string const & name)
    {
        ProfileFunction auto const pos = name.find('.');
        if (pos == std::string::npos)
        {
            return name;
        }
        else
        {
            return name.substr(0, pos);
        }
    }

    // Extract OutputName from "NodeName.OutputName"
    std::string extractOutputName(std::string const & name)
    {
        ProfileFunction auto const pos = name.find('.');
        if (pos == std::string::npos)
        {
            return name;
        }
        else
        {
            return name.substr(pos + 1);
        }
    }

    // make name suitable as a opencl c variable name
    std::string makeValidVariableName(std::string const & name)
    {
        ProfileFunction std::string result;
        for (auto const c : name)
        {
            if (std::isalnum(c))
            {
                result += c;
            }
            else
            {
                result += '_';
            }
        }
        return result;
    }

    void Importer3mf::loadImplicitFunctions(Lib3MF::PModel fileModel, Document & doc)
    {
        ProfileFunction auto resourceIterator = fileModel->GetResources();
        while (resourceIterator->MoveNext())
        {
            auto res = resourceIterator->GetCurrent();
            auto implicitFunc = dynamic_cast<Lib3MF::CImplicitFunction *>(res.get());
            if (implicitFunc)
            {
                processImplicitFunction(doc, implicitFunc);
            }

            auto functionFromImage3d = dynamic_cast<Lib3MF::CFunctionFromImage3D *>(res.get());
            if (functionFromImage3d)
            {
                processFunctionFromImage3d(doc, functionFromImage3d);
            }
        }

        doc.getAssembly()->updateInputsAndOutputs();
    }

    void Importer3mf::loadImplicitFunctionsFiltered(Lib3MF::PModel fileModel,
                                                    Document & doc,
                                                    std::vector<Duplicates> const & duplicates)
    {
        ProfileFunction;

        // If there are no duplicates to filter out, just use the regular method
        if (duplicates.empty())
        {
            loadImplicitFunctions(fileModel, doc);
            return;
        }

        // Create a set of resource IDs to skip (the duplicate functions)
        std::set<Lib3MF_uint32> duplicateIds;
        for (auto const & duplicate : duplicates)
        {
            if (duplicate.duplicateFunction)
            {
                duplicateIds.insert(duplicate.duplicateFunction->GetUniqueResourceID());
            }
        }

        // Log the duplicate IDs
        for (auto const & duplicate : duplicates)
        {
            if (duplicate.duplicateFunction)
            {
                std::cout << "Duplicate function found with ID: "
                          << duplicate.duplicateFunction->GetUniqueResourceID() << std::endl;
            }
        }

        // Process resources, skipping those in the duplicateIds set
        auto resourceIterator = fileModel->GetResources();
        while (resourceIterator->MoveNext())
        {
            auto res = resourceIterator->GetCurrent();
            Lib3MF_uint32 resourceId = res->GetUniqueResourceID();

            // Skip if this resource is in our duplicates list
            if (duplicateIds.find(resourceId) != duplicateIds.end())
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("Skipped loading duplicate function with ID: {}", resourceId),
                       events::Severity::Info});
                }
                continue;
            }

            // Process as usual if not a duplicate
            auto implicitFunc = dynamic_cast<Lib3MF::CImplicitFunction *>(res.get());
            if (implicitFunc)
            {
                processImplicitFunction(doc, implicitFunc);
            }

            auto functionFromImage3d = dynamic_cast<Lib3MF::CFunctionFromImage3D *>(res.get());
            if (functionFromImage3d)
            {
                processFunctionFromImage3d(doc, functionFromImage3d);
            }
        }

        doc.getAssembly()->updateInputsAndOutputs();
    }

    TextureTileStyle toTextureTileStyle(Lib3MF::eTextureTileStyle style)
    {
        ProfileFunction switch (style)
        {
        case Lib3MF::eTextureTileStyle::Wrap:
            return TextureTileStyle::TTS_REPEAT;
        case Lib3MF::eTextureTileStyle::Mirror:
            return TextureTileStyle::TTS_MIRROR;
        case Lib3MF::eTextureTileStyle::Clamp:
            return TextureTileStyle::TTS_CLAMP;
        case Lib3MF::eTextureTileStyle::NoTileStyle:
            return TextureTileStyle::TTS_NONE;
        default:
            return TextureTileStyle::TTS_REPEAT;
        }
    }

    SamplingFilter toSamplingFilter(Lib3MF::eTextureFilter filter)
    {
        ProfileFunction switch (filter)
        {
        case Lib3MF::eTextureFilter::Auto:
            return SF_LINEAR;
        case Lib3MF::eTextureFilter::Linear:
            return SF_LINEAR;
        case Lib3MF::eTextureFilter::Nearest:
            return SF_NEAREST;
        default:
            return SF_LINEAR;
        }
    }

    void Importer3mf::processFunctionFromImage3d(Document & doc,
                                                 Lib3MF::CFunctionFromImage3D * func)
    {
        ProfileFunction if (!func)
        {
            return;
        }

        nodes::SamplingSettings settings{};
        Lib3MF::eTextureTileStyle tileStyleU;
        Lib3MF::eTextureTileStyle tileStyleV;
        Lib3MF::eTextureTileStyle tileStyleW;
        func->GetTileStyles(tileStyleU, tileStyleV, tileStyleW);

        settings.tileStyleU = toTextureTileStyle(tileStyleU);
        settings.tileStyleV = toTextureTileStyle(tileStyleV);
        settings.tileStyleW = toTextureTileStyle(tileStyleW);

        settings.filter = toSamplingFilter(func->GetFilter());

        settings.offset = func->GetOffset();
        settings.scale = func->GetScale();

        nodes::Builder builder;
        builder.createFunctionFromImage3D(*doc.getAssembly(),
                                          func->GetModelResourceID(),
                                          func->GetImage3D()->GetModelResourceID(),
                                          settings);
    }

    void Importer3mf::processImplicitFunction(Document & doc, Lib3MF::CImplicitFunction * func)
    {
        ProfileFunction;

        m_eventLogger = doc.getSharedLogger();

        auto & assembly = *doc.getAssembly();

        if (assembly.findModel(func->GetModelResourceID()))
        {
            return;
        }

        if (m_eventLogger)
        {
            m_eventLogger->addEvent({fmt::format("Creating model: {}", func->GetUniqueResourceID()),
                                     events::Severity::Info});
        }

        assembly.addModelIfNotExisting(func->GetModelResourceID());

        auto & idToNode = m_nodeMaps[func->GetModelResourceID()];
        auto modelIter = doc.getAssembly()->getFunctions().find(func->GetModelResourceID());

        if (modelIter == doc.getAssembly()->getFunctions().end())
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("Failed to create model: {}", func->GetModelResourceID()),
                   events::Severity::Error});
            }
            return;
        }

        auto model = modelIter->second;

        if (!model)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent({fmt::format("Failed to create model, model is null: {}",
                                                     func->GetModelResourceID()),
                                         events::Severity::Error});
            }
            return;
        }

        model->setDisplayName(func->GetDisplayName());
        model->setResourceId(func->GetModelResourceID());

        model->createBeginEnd();
        auto inputIter = func->GetInputs();
        while (inputIter->MoveNext())
        {
            auto input = inputIter->GetCurrent();
            model->addArgument(makeValidVariableName(input->GetIdentifier()),
                               parameterFromType(input->GetType()));
        }

        auto nodeIter = func->GetNodes();
        // create nodes
        while (nodeIter->MoveNext())
        {
            auto node3mf = nodeIter->GetCurrent();

            auto * newNode = createNode(*model, node3mf->GetNodeType());

            if (node3mf->GetNodeType() == Lib3MF::eImplicitNodeType::FunctionCall)
            {
                // add all arguments
                auto inputIter = node3mf->GetInputs();
                while (inputIter->MoveNext())
                {
                    auto input = inputIter->GetCurrent();
                    auto newInput = newNode->addInput(input->GetIdentifier());
                    *newInput = parameterFromType(input->GetType());
                    newInput->setParentId(newNode->getId());
                }

                // add all outputs
                auto outputIter = node3mf->GetOutputs();
                while (outputIter->MoveNext())
                {
                    auto output = outputIter->GetCurrent();
                    newNode->addOutputPort(output->GetIdentifier(),
                                           typeIndexFrom3mfType(output->GetType()));
                }
            }
            newNode->setUniqueName(node3mf->GetIdentifier());
            // tag
            newNode->setTag(node3mf->GetTag());
            model->registerInputs(*newNode);
            model->registerOutputs(*newNode);
            idToNode[makeValidVariableName(node3mf->GetIdentifier())] = newNode;
            newNode->setDisplayName(node3mf->GetDisplayName());
        }

        nodeIter = func->GetNodes();
        // connect nodes
        while (nodeIter->MoveNext())
        {
            auto node3mf = nodeIter->GetCurrent();
            connectNode(*node3mf, *func, *model);
        }

        connectOutputs(*model, *model->getEndNode(), *func);

        model->setLogger(doc.getSharedLogger());

        model->updateTypes();
    }

    void Importer3mf::connectNode(Lib3MF::CImplicitNode & node3mf,
                                  Lib3MF::CImplicitFunction & func,
                                  nodes::Model & model)
    {
        ProfileFunction auto & idToNode = m_nodeMaps[func.GetModelResourceID()];

        auto node = idToNode.at(makeValidVariableName(node3mf.GetIdentifier()));
        auto inputIter = node3mf.GetInputs();
        while (inputIter->MoveNext())
        {
            auto input = inputIter->GetCurrent();
            auto parameterName = makeValidVariableName(input->GetIdentifier());
            auto * parameter = node->getParameter(parameterName);
            bool const nodeIsFunctionCall =
              node3mf.GetNodeType() == Lib3MF::eImplicitNodeType::FunctionCall;
            if (!parameter && nodeIsFunctionCall)
            {
                // create the parameter
                parameter = node->addInput(parameterName);
            }

            if (!parameter)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent({fmt::format("Failed to find parameter {} in node {}",
                                                         parameterName,
                                                         node3mf.GetIdentifier()),
                                             events::Severity::Error});
                }
                continue;
            }

            *parameter = parameterFromType(input->GetType());
            parameter->setParentId(node->getId());
            auto sourcePort = resolveInput(model, input);
            if (sourcePort)
            {
                parameter->setInputFromPort(*sourcePort);
            }
        }

        if (node3mf.GetNodeType() == Lib3MF::eImplicitNodeType::Constant)
        {
            auto scalarNode = dynamic_cast<Lib3MF::CConstantNode *>(&node3mf);

            if (!scalarNode)
            {
                throw std::runtime_error(
                  fmt::format("Could not cast node {} to ConstScalarNode", node->getUniqueName()));
            }
            auto value = scalarNode->GetConstant();
            auto * parameter = node->getParameter(nodes::FieldNames::Value);
            if (parameter)
            {
                parameter->setValue(static_cast<float>(value));
                parameter->setInputSourceRequired(false); // Fix: Constants don't need input sources
            }
        }
        else if (node3mf.GetNodeType() == Lib3MF::eImplicitNodeType::ConstVec)
        {
            auto vectorNode = dynamic_cast<Lib3MF::CConstVecNode *>(&node3mf);
            if (!vectorNode)
            {
                throw std::runtime_error(
                  fmt::format("Could not cast node {} to ConstVecNode", node->getUniqueName()));
            }
            auto value = vectorNode->GetVector();

            int i = 0;
            for (auto const & fieldName :
                 {nodes::FieldNames::X, nodes::FieldNames::Y, nodes::FieldNames::Z})
            {
                auto * parameter = node->getParameter(fieldName);
                if (parameter)
                {
                    parameter->setValue(static_cast<float>(value.m_Coordinates[i]));
                    parameter->setInputSourceRequired(
                      false); // Fix: Constants don't need input sources
                }
                i++;
            }
        }
        else if (node3mf.GetNodeType() == Lib3MF::eImplicitNodeType::ConstMat)
        {
            auto matrixNode = dynamic_cast<Lib3MF::CConstMatNode *>(&node3mf);
            if (!matrixNode)
            {
                throw std::runtime_error(
                  fmt::format("Could not cast node {} to ConstMatNode", node->getUniqueName()));
            }
            auto matrix = matrixNode->GetMatrix();
            int i = 0;
            for (auto const & fieldNames : {nodes::FieldNames::M00,
                                            nodes::FieldNames::M01,
                                            nodes::FieldNames::M02,
                                            nodes::FieldNames::M03,
                                            nodes::FieldNames::M10,
                                            nodes::FieldNames::M11,
                                            nodes::FieldNames::M12,
                                            nodes::FieldNames::M13,
                                            nodes::FieldNames::M20,
                                            nodes::FieldNames::M21,
                                            nodes::FieldNames::M22,
                                            nodes::FieldNames::M23,
                                            nodes::FieldNames::M30,
                                            nodes::FieldNames::M31,
                                            nodes::FieldNames::M32,
                                            nodes::FieldNames::M33})
            {
                auto * parameter = node->getParameter(fieldNames);
                auto col = i % 4;
                auto row = i / 4;
                if (parameter)
                {
                    parameter->setValue(static_cast<float>(matrix.m_Field[row][col]));
                    parameter->setInputSourceRequired(
                      false); // Fix: Constants don't need input sources
                }
                i++;
            }
        }
        else if (node3mf.GetNodeType() == Lib3MF::eImplicitNodeType::ConstResourceID)
        {
            auto resourceNode3mf = dynamic_cast<Lib3MF::CResourceIdNode *>(&node3mf);
            if (!resourceNode3mf)
            {
                throw std::runtime_error(
                  fmt::format("Could not cast node {} to ResourceIdNode", node3mf.GetIdentifier()));
            }
            auto resourceNode = dynamic_cast<nodes::Resource *>(&*node);
            if (!resourceNode)
            {
                throw std::runtime_error(
                  fmt::format("Could not cast node {} to ResourceNode", node->getUniqueName()));
            }
            auto resource = resourceNode3mf->GetResource();

            if (!resource)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("Resource not found: {}", node3mf.GetIdentifier()),
                       events::Severity::Warning});
                }
                return;
            }
            auto resourceId = resource->GetModelResourceID();
            resourceNode->setResourceId(resourceId);
        }
    }

    void Importer3mf::connectOutputs(gladius::nodes::Model & model,
                                     gladius::nodes::NodeBase & endNode,
                                     Lib3MF::CImplicitFunction & func)
    {
        ProfileFunction
        {

            auto outputIter = func.GetOutputs();
            while (outputIter->MoveNext())
            {
                auto output = outputIter->GetCurrent();

                auto parameterName = makeValidVariableName(output->GetIdentifier());
                auto parameter = endNode.parameter()[parameterName] =
                  parameterFromType(output->GetType());
            }

            model.registerInputs(endNode);
        }
        {
            auto outputIter = func.GetOutputs();
            while (outputIter->MoveNext())
            {
                auto output = outputIter->GetCurrent();

                auto parameterName = makeValidVariableName(output->GetIdentifier());
                auto parameter = endNode.getParameter(parameterName);
                if (!parameter)
                {
                    if (m_eventLogger)
                    {
                        m_eventLogger->addEvent(
                          {fmt::format("Could not find parameter {} of function output",
                                       output->GetIdentifier()),
                           events::Severity::Warning});
                    }
                    continue;
                }

                auto sourcePort = resolveInput(model, output);
                if (sourcePort)
                {
                    if (!model.addLink(sourcePort->getId(), parameter->getId()))
                    {
                        if (m_eventLogger)
                        {
                            m_eventLogger->addEvent({fmt::format("Could not add link from {} to {}",
                                                                 sourcePort->getUniqueName(),
                                                                 parameterName),
                                                     events::Severity::Warning});
                        }
                    }
                }
                else
                {
                    if (m_eventLogger)
                    {
                        m_eventLogger->addEvent(
                          {fmt::format("Could not resolve input for {} of function output",
                                       output->GetIdentifier()),
                           events::Severity::Warning});
                    }
                }
            }
        }
    }

    nodes::Port * Importer3mf::resolveInput(nodes::Model & model, Lib3MF::PImplicitPort & input)
    {
        ProfileFunction

          auto refName = input->GetReference();
        if (refName.empty())
        {
            return nullptr;
        }
        auto sourceNodeName = makeValidVariableName(extractNodeName(refName));
        auto & idToNode = m_nodeMaps.at(model.getResourceId());

        gladius::nodes::NodeBase * sourceNode = nullptr;
        if (sourceNodeName == "inputs")
        {
            sourceNode = model.getBeginNode();
        }
        else
        {
            auto sourceNodeIter = idToNode.find(sourceNodeName);
            if (sourceNodeIter == idToNode.end())
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent({fmt::format("Node not found: {}\n", sourceNodeName),
                                             events::Severity::Error});
                }
                return nullptr;
            }
            sourceNode = sourceNodeIter->second;
        }

        auto sourcePortName = makeValidVariableName(extractOutputName(refName));
        auto sourcePortIter = sourceNode->getOutputs().find(sourcePortName);
        if (sourcePortIter == sourceNode->getOutputs().end())
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format(
                     "Resolving {} failed. Port of node {} not found: {}. Did you mean {}?\n",
                     refName,
                     sourceNodeName,
                     sourcePortName,
                     sourceNode->getOutputs().begin()->first),
                   events::Severity::Error});
            }

            return nullptr;
        }
        return &sourcePortIter->second;
    }

    Vector3 toVector3(Lib3MF::sPosition const & a)
    {
        return Vector3(a.m_Coordinates[0], a.m_Coordinates[1], a.m_Coordinates[2]);
    }

    openvdb::Vec3s toOpenVdbVector(Lib3MF::sPosition const & a)
    {
        return openvdb::Vec3s(a.m_Coordinates[0], a.m_Coordinates[1], a.m_Coordinates[2]);
    }

    nodes::Matrix4x4 matrix4x4From3mfTransform(Lib3MF::sTransform const & transform)
    {
        nodes::Matrix4x4 mat = identityMatrix();

        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 3; ++col)
            {
                mat[row][col] = transform.m_Fields[row][col];
            }
        }

        return mat;
    }

    std::vector<Lib3MF::PImplicitFunction>
    Importer3mf::collectImplicitFunctions(Lib3MF::PModel const & model) const
    {
        ProfileFunction std::vector<Lib3MF::PImplicitFunction> implicitFunctions;
        auto resourceIterator = model->GetResources();

        while (resourceIterator->MoveNext())
        {
            auto resource = resourceIterator->GetCurrent();
            auto implicitFunc = std::dynamic_pointer_cast<Lib3MF::CImplicitFunction>(resource);

            if (implicitFunc)
            {
                implicitFunctions.push_back(implicitFunc);
            }
        }

        return implicitFunctions;
    }

    std::vector<Duplicates> Importer3mf::findDuplicatedFunctions(
      std::vector<Lib3MF::PImplicitFunction> const & originalFunctions,
      Lib3MF::PModel const & extendedModel) const
    {
        ProfileFunction std::vector<Duplicates> duplicates;

        // For each original function, search for an equivalent in the extended model
        for (const auto & originalFunction : originalFunctions)
        {
            // Skip if function is null (should not happen, but let's be safe)
            if (!originalFunction)
            {
                continue;
            }

            // Use the FunctionComparator to find an equivalent function
            auto equivalentFunction = findEquivalentFunction(*extendedModel, *originalFunction);

            // If an equivalent function is found, store the function pointers in the result
            if (equivalentFunction)
            {
                Duplicates duplicate{originalFunction, equivalentFunction};

                duplicates.push_back(duplicate);
            }
        }

        return duplicates;
    }

    std::set<Lib3MF_uint32>
    Importer3mf::collectFunctionResourceIds(Lib3MF::PModel const & model) const
    {
        ProfileFunction std::set<Lib3MF_uint32> functionResourceIds;
        auto resourceIterator = model->GetResources();
        while (resourceIterator->MoveNext())
        {
            auto resource = resourceIterator->GetCurrent();
            auto implicitFunc = dynamic_cast<Lib3MF::CImplicitFunction *>(resource.get());
            auto functionFromImage3d = dynamic_cast<Lib3MF::CFunctionFromImage3D *>(resource.get());

            if (implicitFunc || functionFromImage3d)
            {
                functionResourceIds.insert(resource->GetResourceID());
            }
        }
        return functionResourceIds;
    }

    void
    Importer3mf::replaceDuplicatedFunctionReferences(std::vector<Duplicates> const & duplicates,
                                                     Lib3MF::PModel const & model) const
    {
        ProfileFunction

          // If no duplicates were found, no need to replace anything
          if (duplicates.empty())
        {
            return;
        }

        // Get all implicit functions from the model
        auto resourceIterator = model->GetResources();

        // Iterate through all resources
        while (resourceIterator->MoveNext())
        {
            auto resource = resourceIterator->GetCurrent();

            // Check if the resource is an implicit function
            auto implicitFunction = std::dynamic_pointer_cast<Lib3MF::CImplicitFunction>(resource);
            if (!implicitFunction)
            {
                // Not an implicit function, skip
                continue;
            }

            // Get all nodes in this function
            auto nodeIterator = implicitFunction->GetNodes();

            // Iterate through all nodes in the function
            while (nodeIterator->MoveNext())
            {
                auto node = nodeIterator->GetCurrent();

                // Check if this is a ResourceIdNode (ConstResourceID type)
                if (node->GetNodeType() == Lib3MF::eImplicitNodeType::ConstResourceID)
                {
                    auto resourceIdNode = std::dynamic_pointer_cast<Lib3MF::CResourceIdNode>(node);
                    if (!resourceIdNode)
                    {
                        if (m_eventLogger)
                        {
                            m_eventLogger->addEvent(
                              {fmt::format("Could not cast node {} to ResourceIdNode",
                                           node->GetIdentifier()),
                               events::Severity::Warning});
                        }
                        continue;
                    }

                    // Get the resource referenced by this node
                    Lib3MF::PResource referencedResource;
                    try
                    {
                        referencedResource = resourceIdNode->GetResource();
                    }
                    catch (const std::exception & e)
                    {
                        if (m_eventLogger)
                        {
                            m_eventLogger->addEvent(
                              {fmt::format("Error retrieving resource from ResourceIdNode {}: {}",
                                           node->GetIdentifier(),
                                           e.what()),
                               events::Severity::Warning});
                        }
                        continue;
                    }

                    if (!referencedResource)
                    {
                        if (m_eventLogger)
                        {
                            m_eventLogger->addEvent(
                              {fmt::format("ResourceIdNode {} references a null resource",
                                           node->GetIdentifier()),
                               events::Severity::Warning});
                        }
                        continue;
                    }

                    // Check if this resource is one of our duplicate functions
                    for (const auto & duplicate : duplicates)
                    {
                        // If the referenced resource ID matches a duplicate function's ID
                        if (referencedResource->GetUniqueResourceID() ==
                            duplicate.duplicateFunction->GetUniqueResourceID())
                        {
                            // Replace the reference with the original function
                            try
                            {
                                auto originalResource = model->GetResourceByID(
                                  duplicate.originalFunction->GetUniqueResourceID());
                                resourceIdNode->SetResource(originalResource);

                                if (m_eventLogger)
                                {
                                    m_eventLogger->addEvent(
                                      {fmt::format(
                                         "Replaced reference to duplicate function {} "
                                         "with original function {}",
                                         duplicate.duplicateFunction->GetUniqueResourceID(),
                                         duplicate.originalFunction->GetUniqueResourceID()),
                                       events::Severity::Info});
                                }
                            }
                            catch (const std::exception & e)
                            {
                                if (m_eventLogger)
                                {
                                    m_eventLogger->addEvent(
                                      {fmt::format(
                                         "Error replacing function reference in node {}: {}",
                                         node->GetIdentifier(),
                                         e.what()),
                                       events::Severity::Error});
                                }
                            }

                            // We've handled this node, no need to check other duplicates
                            break;
                        }
                    }
                }
            }
        }
    }

    void Importer3mf::loadMeshes(Lib3MF::PModel model, Document & doc)
    {
        ProfileFunction auto objectIterator = model->GetObjects();
        while (objectIterator->MoveNext())
        {
            auto const object = objectIterator->GetCurrentObject();

            if (object->IsMeshObject())
            {
                auto const meshObj = model->GetMeshObjectByID(object->GetUniqueResourceID());
                loadMeshIfNecessary(model, meshObj, doc);
            }
            else
            {
                // Skip non-mesh objects
            }
        }
    }

    void Importer3mf::loadComponentObjects(Lib3MF::PModel model, Document & doc)
    {
        ProfileFunction auto objectIterator = model->GetObjects();
        nodes::Builder builder;
        float const units_per_mm = computeUnitsPerMM(model);
        while (objectIterator->MoveNext())
        {
            auto const object = objectIterator->GetCurrentObject();

            if (object->IsComponentsObject())
            {
                nodes::Components components;
                auto const compObjs = model->GetComponentsObjectByID(object->GetUniqueResourceID());

                for (Lib3MF_uint32 i = 0; i < compObjs->GetComponentCount(); ++i)
                {
                    auto const component = compObjs->GetComponent(i);
                    components.push_back({component->GetObjectResourceID(),
                                          matrix4x4From3mfTransform(component->GetTransform())});
                }

                builder.addCompositeModel(doc, object->GetResourceID(), components, units_per_mm);
            }
        }
    }

    void Importer3mf::loadMeshIfNecessary(Lib3MF::PModel model,
                                          Lib3MF::PMeshObject meshObject,
                                          Document & doc)
    {
        ProfileFunction auto key =
          ResourceKey(static_cast<uint32_t>(meshObject->GetModelResourceID()), ResourceType::Mesh);
        key.setDisplayName(meshObject->GetName());

        if (doc.getGeneratorContext().resourceManager.hasResource(key))
        {
            return;
        }

        vdb::TriangleMesh mesh;

        auto const numFaces = meshObject->GetTriangleCount();

        for (auto faceIndex = 0u; faceIndex < numFaces; ++faceIndex)
        {
            auto const & triangle = meshObject->GetTriangle(faceIndex);
            auto const a = toOpenVdbVector(meshObject->GetVertex(triangle.m_Indices[0]));
            auto const b = toOpenVdbVector(meshObject->GetVertex(triangle.m_Indices[1]));
            auto const c = toOpenVdbVector(meshObject->GetVertex(triangle.m_Indices[2]));
            mesh.addTriangle(a, b, c);
        }

        if (mesh.indices.size() == 0u)
        {
            // Still check for beam lattice even if mesh has no triangles
            loadBeamLatticeIfNecessary(model, meshObject, doc);
            return;
        }
        doc.getGeneratorContext().resourceManager.addResource(key, std::move(mesh));

        // Also load beam lattice if present
        loadBeamLatticeIfNecessary(model, meshObject, doc);
    }

    void Importer3mf::loadBeamLatticeIfNecessary(Lib3MF::PModel model,
                                                 Lib3MF::PMeshObject meshObject,
                                                 Document & doc)
    {
        ProfileFunction

        try
        {
            // Create resource key for beam lattice (use same resource ID but different type)
            auto key = ResourceKey(static_cast<uint32_t>(meshObject->GetModelResourceID()),
                                   ResourceType::BeamLattice);
            key.setDisplayName(meshObject->GetName() + "_BeamLattice");

            // Check if beam lattice resource already exists
            if (doc.getGeneratorContext().resourceManager.hasResource(key))
            {
                return;
            }

            // Use the new BeamLatticeImporter for unified processing
            BeamLatticeImporter importer(m_eventLogger);
            if (!importer.process(meshObject))
            {
                return; // No beam lattice or processing failed
            }

            // Get processed data from importer
            const auto & beams = importer.getBeams();
            const auto & balls = importer.getBalls();
            const auto & ballConfig = importer.getBallConfig();

            if (beams.empty())
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("BeamLattice {} has no beams", meshObject->GetModelResourceID()),
                       gladius::events::Severity::Warning});
                }
                return;
            }

            // Read clipping information from beam lattice (separate from main processing)
            Lib3MF::eBeamLatticeClipMode clippingMode = Lib3MF::eBeamLatticeClipMode::NoClipMode;
            Lib3MF_uint32 clippingMeshResourceId = 0;

            try
            {
                Lib3MF::PBeamLattice beamLattice = meshObject->BeamLattice();
                if (beamLattice)
                {
                    beamLattice->GetClipping(clippingMode, clippingMeshResourceId);
                }
            }
            catch (const std::exception & e)
            {
                // If clipping info is not available, continue with no clipping
                clippingMode = Lib3MF::eBeamLatticeClipMode::NoClipMode;
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {fmt::format(
                         "Warning: Could not read clipping information from beam lattice {}: {}",
                         meshObject->GetModelResourceID(),
                         e.what()),
                       gladius::events::Severity::Warning});
                }
            }

            // Create beam lattice resource with processed data
            auto beamLatticeResource =
              std::make_unique<BeamLatticeResource>(key,
                                                    std::move(std::vector<BeamData>(beams)),
                                                    std::move(std::vector<BallData>(balls)),
                                                    ballConfig);

            // Update display name to include ball information
            std::string displayName = meshObject->GetName() + "_BeamLattice";
            if (!balls.empty())
            {
                displayName += fmt::format(" ({} balls)", balls.size());
            }
            key.setDisplayName(displayName);

            // Add the resource to the manager
            doc.getGeneratorContext().resourceManager.addResource(key,
                                                                  std::move(beamLatticeResource));

            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("Successfully loaded beam lattice {} with {} beams and {} balls",
                               meshObject->GetModelResourceID(),
                               beams.size(),
                               balls.size()),
                   gladius::events::Severity::Info});
            }
        }
        catch (const std::exception & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent({fmt::format("Error loading beam lattice from mesh {}: {}",
                                                     meshObject->GetModelResourceID(),
                                                     e.what()),
                                         gladius::events::Severity::Error});
            }
        }
    }

    BoundingBox Importer3mf::computeBoundingBox(Lib3MF::PMeshObject mesh)
    {
        ProfileFunction BoundingBox bbox;
        bbox.min = {std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    std::numeric_limits<float>::max(),
                    1.f};
        bbox.max = {-std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(),
                    -std::numeric_limits<float>::max(),
                    1.f};

        for (auto i = 0u; i < mesh->GetVertexCount(); ++i)
        {
            auto const vertex = mesh->GetVertex(i);

            bbox.min.x = std::min(bbox.min.x, vertex.m_Coordinates[0]);
            bbox.min.y = std::min(bbox.min.y, vertex.m_Coordinates[1]);
            bbox.min.z = std::min(bbox.min.z, vertex.m_Coordinates[2]);

            bbox.max.x = std::max(bbox.max.x, vertex.m_Coordinates[0]);
            bbox.max.y = std::max(bbox.max.y, vertex.m_Coordinates[1]);
            bbox.max.z = std::max(bbox.max.z, vertex.m_Coordinates[2]);
        }
        return bbox;
    }

    void Importer3mf::addMeshObject(Lib3MF::PModel model,
                                    ResourceKey const & key,
                                    Lib3MF::PMeshObject mesh,
                                    nodes::Matrix4x4 const & trafo,
                                    Document & doc)
    {
        nodes::Builder builder;
        bool requiresMesh = true;
        if (mesh)
        {
            auto volume = mesh->GetVolumeData();
            float const units_per_mm = computeUnitsPerMM(model);
            auto coordinateSystemPort = builder.addTransformationToInputCs(
              *doc.getAssembly()->assemblyModel(), trafo, units_per_mm);

            if (requiresMesh)
            {
                builder.addResourceRef(
                  *doc.getAssembly()->assemblyModel(), key, coordinateSystemPort);
            }

            if (volume)
            {
                addVolumeData(volume, model, doc, builder, coordinateSystemPort);
            }
        }
    }

    void Importer3mf::addBeamLatticeObject(Lib3MF::PModel model,
                                           ResourceKey const & key,
                                           Lib3MF::PMeshObject meshObject,
                                           nodes::Matrix4x4 const & trafo,
                                           Document & doc)
    {
        // BEAM_LATTICE_VERIFICATION_MARKER_2025_09_05
        nodes::Builder builder;

        if (meshObject)
        {
            // Check if mesh object has a beam lattice (same pattern as in
            // loadBeamLatticeIfNecessary)
            Lib3MF::PBeamLattice beamLattice = meshObject->BeamLattice();
            if (beamLattice)
            {
                float const units_per_mm = computeUnitsPerMM(model);
                auto coordinateSystemPort = builder.addTransformationToInputCs(
                  *doc.getAssembly()->assemblyModel(), trafo, units_per_mm);

                // Read clipping information from beam lattice
                Lib3MF::eBeamLatticeClipMode clippingMode =
                  Lib3MF::eBeamLatticeClipMode::NoClipMode;
                Lib3MF_uint32 clippingMeshResourceId = 0;

                try
                {
                    beamLattice->GetClipping(clippingMode, clippingMeshResourceId);
                }
                catch (const std::exception & e)
                {
                    // If clipping info is not available, continue with no clipping
                    clippingMode = Lib3MF::eBeamLatticeClipMode::NoClipMode;
                    if (m_eventLogger)
                    {
                        m_eventLogger->addEvent({fmt::format("Warning: Could not read clipping "
                                                             "information from beam lattice {}: {}",
                                                             meshObject->GetModelResourceID(),
                                                             e.what()),
                                                 gladius::events::Severity::Warning});
                    }
                }

                // Handle clipping based on the mode
                if (clippingMode == Lib3MF::eBeamLatticeClipMode::NoClipMode)
                {
                    // No clipping - use the current behavior
                    builder.addBeamLatticeRef(
                      *doc.getAssembly()->assemblyModel(), key, coordinateSystemPort);
                }
                else
                {
                    // Clipping required - get clipping mesh resource key
                    auto clippingMeshKey = ResourceKey(clippingMeshResourceId, ResourceType::Mesh);

                    // Check if clipping mesh resource exists
                    if (!doc.getGeneratorContext().resourceManager.hasResource(clippingMeshKey))
                    {
                        if (m_eventLogger)
                        {
                            m_eventLogger->addEvent(
                              {fmt::format(
                                 "Error: Clipping mesh resource {} not found for beam lattice {}",
                                 clippingMeshResourceId,
                                 meshObject->GetModelResourceID()),
                               gladius::events::Severity::Error});
                        }
                        // Fallback to no clipping
                        builder.addBeamLatticeRef(
                          *doc.getAssembly()->assemblyModel(), key, coordinateSystemPort);
                    }
                    else
                    {
                        // Apply clipping
                        builder.addBeamLatticeWithClipping(*doc.getAssembly()->assemblyModel(),
                                                           key,
                                                           clippingMeshKey,
                                                           static_cast<int>(clippingMode),
                                                           coordinateSystemPort);
                    }
                }
            }
        }
    }

    void Importer3mf::addVolumeData(Lib3MF::PVolumeData & volume,
                                    Lib3MF::PModel & model,
                                    gladius::Document & doc,
                                    gladius::nodes::Builder & builder,
                                    gladius::nodes::Port & coordinateSystemPort)
    {
        auto color = volume->GetColor();
        if (color)
        {
            auto funcId = color->GetFunctionResourceID();
            auto res = model->GetResourceByID(funcId);
            if (!res)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("Could not find color function with unique id {} "
                                   "to resolve the model id",
                                   funcId),
                       events::Severity::Error});
                }
                return;
            }
            auto modelFuncId = res->GetModelResourceID();

            auto colorFunction = doc.getAssembly()->findModel(modelFuncId);
            if (!colorFunction)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("Could not find color function with id {}", modelFuncId),
                       events::Severity::Error});
                }
                return;
            }

            auto transform = matrix4x4From3mfTransform(color->GetTransform());
            builder.appendFunctionForColorOutput(
              *doc.getAssembly()->assemblyModel(), *colorFunction, coordinateSystemPort, transform);

            doc.getAssembly()->updateInputsAndOutputs();
        }
    }

    void Importer3mf::addLevelSetObject(Lib3MF::PModel model,
                                        ResourceKey const & key,
                                        Lib3MF::PLevelSet levelSet,
                                        nodes::Matrix4x4 const & trafo,
                                        Document & doc)
    {
        nodes::Builder builder;
        if (levelSet)
        {
            auto function = levelSet->GetFunction();
            if (!function)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {"No function found for level set", events::Severity::Error});
                }
                return;
            }

            auto funcId = levelSet->GetFunction()->GetResourceID();
            auto res = model->GetResourceByID(funcId);
            if (!res)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent({fmt::format("Could not find function with model id {} "
                                                         "to resolve the model id",
                                                         funcId),
                                             events::Severity::Error});
                }
                return;
            }
            auto modelFuncId = res->GetModelResourceID();

            auto gladiusFunction = doc.getAssembly()->findModel(modelFuncId);
            if (!gladiusFunction)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {fmt::format("Could not find boundary function with id {}", modelFuncId),
                       events::Severity::Error});
                }
                return;
            }
            auto channelName = levelSet->GetChannelName();
            if (channelName.empty())
            {
                channelName = "shape";
            }

            float const units_per_mm = computeUnitsPerMM(model);
            auto buildItemCoordinateSystemPort = builder.addTransformationToInputCs(
              *doc.getAssembly()->assemblyModel(), trafo, units_per_mm);

            auto levelSetTransform = levelSet->GetTransform();
            auto levelSetTrafo = matrix4x4From3mfTransform(levelSetTransform);

            auto levelSetCoordinateSystemPort =
              builder.insertTransformation(*doc.getAssembly()->assemblyModel(),
                                           buildItemCoordinateSystemPort,
                                           levelSetTrafo,
                                           1.0f);

            auto mesh = levelSet->GetMesh();
            if (!mesh)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {"No mesh found for level set", events::Severity::Error});
                }
                return;
            }

            // Calculate the bounding box for this level set
            BoundingBox bbox;
            if (levelSet->GetMeshBBoxOnly())
            {
                bbox = computeBoundingBox(mesh);
            }
            else
            {

                bbox = computeBoundingBox(mesh);
                // Also load the mesh reference if needed
                auto referencedMeshKey =
                  ResourceKey(mesh->GetModelResourceID(), ResourceType::Mesh);
                loadMeshIfNecessary(model, mesh, doc);
            }

            // Use the new method that creates a complete level set operation:
            // (function ∩ bounding_box) and unions it with existing level sets
            builder.addLevelSetWithDomain(*doc.getAssembly()->assemblyModel(),
                                          *gladiusFunction,
                                          levelSetCoordinateSystemPort,
                                          channelName,
                                          bbox,
                                          buildItemCoordinateSystemPort);

            doc.getAssembly()->setFallbackValueLevelSet((levelSet->GetFallBackValue()));

            auto volumeData = levelSet->GetVolumeData();

            if (volumeData)
            {
                addVolumeData(volumeData, model, doc, builder, levelSetCoordinateSystemPort);
            }

            doc.getAssembly()->updateInputsAndOutputs();
        }
    }

    void Importer3mf::loadImageStacks(std::filesystem::path const & filename,
                                      Lib3MF::PModel model,
                                      Document & doc)
    {
        ProfileFunction auto image3dIterator = model->GetImage3Ds();

        ImageExtractor extractor;
        if (!extractor.loadFromArchive(filename))
        {
            throw std::runtime_error(fmt::format("Could not open file {}", filename.string()));
        }
        extractor.printAllFiles();

        try
        {
            while (image3dIterator->MoveNext())
            {
                auto image3d = image3dIterator->GetCurrentImage3D();
                auto & resMan = doc.getGeneratorContext().resourceManager;

                if (image3d->IsImageStack())
                {
                    auto imageStack3mf = model->GetImageStackByID(image3d->GetUniqueResourceID());
                    FileList fileList;
                    for (auto index = 0u; index < imageStack3mf->GetSheetCount(); ++index)
                    {
                        auto sheet = imageStack3mf->GetSheet(index);
                        if (!sheet)
                        {
                            if (m_eventLogger)
                            {
                                m_eventLogger->addEvent(
                                  {"Sheet happens: Sheet not found", events::Severity::Error});
                            }
                            continue;
                        }
                        fileList.push_back(sheet->GetPath());
                    }

                    bool const useVdb = extractor.determinePixelFormat(fileList.front()) ==
                                        PixelFormat::GRAYSCALE_8BIT;

                    ResourceType resourceType =
                      useVdb ? ResourceType::Vdb : ResourceType::ImageStack;
                    auto key = ResourceKey{image3d->GetModelResourceID(), resourceType};

                    // Check if resource already exists
                    if (resMan.hasResource(key))
                    {
                        continue;
                    }

                    key.setDisplayName(image3d->GetName());
                    if (useVdb)
                    {
                        auto grid = extractor.loadAsVdbGrid(fileList, FileLoaderType::Archive);

                        resMan.addResource(key, std::move(grid));
                    }
                    else
                    {

                        auto imageStack = extractor.loadImageStack(fileList);
                        imageStack.setResourceId(image3d->GetModelResourceID());

                        resMan.addResource(key, std::move(imageStack));
                    }
                }
            }
        }
        catch (const std::exception & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("Error while loading image stack: {}", e.what()),
                   events::Severity::Error});
            }
            else
            {
                std::cerr << "Error while loading image stack: " << e.what() << std::endl;
            }
        }
    }

    void Importer3mf::loadBuildItems(Lib3MF::PModel model, Document & doc)
    {
        ProfileFunction;

        doc.getAssembly()->assemblyModel()->setManaged(true);
        auto buildItemIterator = model->GetBuildItems();
        while (buildItemIterator->MoveNext())
        {
            auto currentBuildItem = buildItemIterator->GetCurrent();
            nodes::Matrix4x4 const transformation =
              matrix4x4From3mfTransform(currentBuildItem->GetObjectTransform());
            nodes::Matrix4x4 const trafo = inverseMatrix(transformation);

            auto buildItemIter = doc.addBuildItem({currentBuildItem->GetObjectResourceID(),
                                                   transformation,
                                                   currentBuildItem->GetPartNumber()});

            auto objectRes = currentBuildItem->GetObjectResource();

            if (!objectRes)
            {
                if (m_eventLogger)
                {
                    m_eventLogger->addEvent(
                      {"No object resource for build item", events::Severity::Error});
                }
                continue;
            }

            if (objectRes->IsComponentsObject())
            {
                auto compObjs = model->GetComponentsObjectByID(objectRes->GetUniqueResourceID());
                if (!compObjs)
                {
                    if (m_eventLogger)
                    {
                        m_eventLogger->addEvent(
                          {"No components object for build item", events::Severity::Error});
                    }
                    continue;
                }

                // loop over components
                for (Lib3MF_uint32 i = 0; i < compObjs->GetComponentCount(); ++i)
                {
                    auto component = compObjs->GetComponent(i);

                    auto componentObj = component->GetObjectResource();
                    if (!componentObj)
                    {
                        if (m_eventLogger)
                        {
                            m_eventLogger->addEvent(
                              {"No components object for component", events::Severity::Error});
                        }
                        continue;
                    }

                    nodes::Matrix4x4 componentTrafo = identityMatrix();

                    if (component->HasTransform())
                    {
                        auto const transformationComponent =
                          matrix4x4From3mfTransform(component->GetTransform());
                        componentTrafo = inverseMatrix(transformationComponent);
                    }

                    buildItemIter->addComponent(
                      {componentObj->GetModelResourceID(), componentTrafo});
                    auto key = ResourceKey{componentObj->GetModelResourceID()};
                    key.setDisplayName(componentObj->GetName());

                    // Check if this component has a beam lattice to determine the correct resource
                    // type
                    if (componentObj->IsMeshObject())
                    {
                        auto mesh = model->GetMeshObjectByID(componentObj->GetUniqueResourceID());
                        if (mesh && mesh->BeamLattice())
                        {
                            // This is a beam lattice component - create key with BeamLattice type
                            key =
                              ResourceKey{static_cast<uint32_t>(componentObj->GetModelResourceID()),
                                          ResourceType::BeamLattice};
                            key.setDisplayName(componentObj->GetName() + "_BeamLattice");
                        }
                    }

                    createObject(*componentObj, model, key, componentTrafo, doc);
                }
            }
            else
            {
                auto key = ResourceKey{objectRes->GetModelResourceID(), ResourceType::Mesh};
                key.setDisplayName(currentBuildItem->GetObjectResource()->GetName());

                // Check if this object has a beam lattice to determine the correct resource type
                if (objectRes->IsMeshObject())
                {
                    auto mesh = model->GetMeshObjectByID(objectRes->GetUniqueResourceID());
                    if (mesh && mesh->BeamLattice())
                    {
                        // This is a beam lattice object - create key with BeamLattice type
                        key = ResourceKey{static_cast<uint32_t>(objectRes->GetModelResourceID()),
                                          ResourceType::BeamLattice};
                        key.setDisplayName(currentBuildItem->GetObjectResource()->GetName() +
                                           "_BeamLattice");
                    }
                }

                createObject(*objectRes, model, key, trafo, doc);
            }
        }

        // Normalize distances to mm via Builder helper
        nodes::Builder::applyDistanceNormalization(*doc.getAssembly()->assemblyModel(),
                                                   computeUnitsPerMM(model));
    }

    void Importer3mf::createObject(Lib3MF::CObject & objectRes,
                                   Lib3MF::PModel & model,
                                   gladius::ResourceKey & key,
                                   const gladius::nodes::Matrix4x4 & trafo,
                                   gladius::Document & doc)
    {
        if (objectRes.IsMeshObject())
        {
            auto mesh = model->GetMeshObjectByID(objectRes.GetUniqueResourceID());

            // Check for beam lattice first (using same pattern as loadBeamLatticeIfNecessary)
            if (mesh)
            {
                if (mesh->GetTriangleCount() > 0)
                {
                    addMeshObject(model, key, mesh, trafo, doc);
                }
                Lib3MF::PBeamLattice beamLattice = mesh->BeamLattice();
                if (beamLattice)
                {
                    addBeamLatticeObject(model, key, mesh, trafo, doc);
                }
            }
        }
        else if (objectRes.IsLevelSetObject())
        {
            auto levelSet = model->GetLevelSetByID(objectRes.GetUniqueResourceID());
            addLevelSetObject(model, key, levelSet, trafo, doc);
        }
    }

    void Importer3mf::load(std::filesystem::path const & filename, Document & doc)
    {
        ProfileFunction

          doc.newEmptyModel();

        auto const model = m_wrapper->CreateModel();
        auto const reader = model->QueryReader("3mf");
        doc.set3mfModel(model);

        reader->SetStrictModeActive(false);
        try
        {
            reader->ReadFromFile(filename.string());
        }
        catch (Lib3MF::ELib3MFException const & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent({fmt::format("Error #{} while reading 3mf file {}: {}",
                                                     filename.string(),
                                                     e.getErrorCode(),
                                                     e.what()),
                                         events::Severity::Error});
            }
        }

        logWarnings(filename, reader);

        loadImageStacks(filename, model, doc);
        loadImplicitFunctions(model, doc);
        loadMeshes(model, doc);
        // loadComponentObjects(model, doc);
        loadBuildItems(model, doc);
    }

    void Importer3mf::merge(std::filesystem::path const & filename, Document & doc)
    {
        ProfileFunction auto targetModel = doc.get3mfModel();
        if (!targetModel)
        {
            load(filename, doc);
            return;
        }

        auto core = doc.getCore();
        auto computenToken = core->waitForComputeToken();

        auto const modelToMergeFrom = m_wrapper->CreateModel();
        auto const reader = modelToMergeFrom->QueryReader("3mf");

        reader->SetStrictModeActive(false);
        try
        {
            reader->ReadFromFile(filename.string());
        }
        catch (Lib3MF::ELib3MFException const & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent({fmt::format("Error #{} while reading 3mf file {}: {}",
                                                     filename.string(),
                                                     e.getErrorCode(),
                                                     e.what()),
                                         events::Severity::Error});
            }
            logWarnings(filename, reader);
            return;
        }

        logWarnings(filename, reader);

        try
        {
            // backup the list of function ids

            std::set<Lib3MF_uint32> functionResourceIds = collectFunctionResourceIds(targetModel);
            // store the ptr to the original functions
            auto implicitFunctions = collectImplicitFunctions(targetModel);

            targetModel->MergeFromModel(modelToMergeFrom.get());

            // now find all duplicated functions
            size_t numDuplicatesPrevious = 0;
            size_t numDuplicatesCurrent = 0;
            std::vector<Duplicates> duplicates;
            do
            {
                numDuplicatesPrevious = numDuplicatesCurrent;

                duplicates = findDuplicatedFunctions(implicitFunctions, targetModel);
                numDuplicatesCurrent = duplicates.size();

                // replace the references to the duplicated functions with the original ones
                replaceDuplicatedFunctionReferences(duplicates, targetModel);

            } while (numDuplicatesPrevious != numDuplicatesCurrent);

            // remove the duplicates from the model
            for (auto const & duplicate : duplicates)
            {
                // log
                m_eventLogger->addEvent({fmt::format("Removed resource from model3mf: {}",
                                                     duplicate.duplicateFunction->GetResourceID()),
                                         events::Severity::Info});
                // targetModel->RemoveResource(duplicate.duplicateFunction.get());
                auto const & resource =
                  targetModel->GetResourceByID(duplicate.duplicateFunction->GetUniqueResourceID());
                if (!resource)
                {
                    if (m_eventLogger)
                    {
                        m_eventLogger->addEvent(
                          {fmt::format("Resource {} not found in model3mf",
                                       duplicate.duplicateFunction->GetUniqueResourceID()),
                           events::Severity::Error});
                    }
                    continue;
                }
                targetModel->RemoveResource(resource);
            }

            loadImageStacks(filename, targetModel, doc);
            loadImplicitFunctionsFiltered(targetModel, doc, duplicates);

            doc.rebuildResourceDependencyGraph();
        }
        catch (Lib3MF::ELib3MFException const & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent({fmt::format("Error #{} while merging 3mf file {}: {}",
                                                     filename.string(),
                                                     e.getErrorCode(),
                                                     e.what()),
                                         events::Severity::Error});
            }
        }
        catch (std::exception const & e)
        {
            if (m_eventLogger)
            {
                m_eventLogger->addEvent(
                  {fmt::format("Error while merging 3mf file {}: {}", filename.string(), e.what()),
                   events::Severity::Error});
            }
        }
    }

    Lib3MF::PWrapper Importer3mf::get3mfWrapper() const
    {
        return m_wrapper;
    }

    void loadFrom3mfFile(std::filesystem::path const filename, Document & doc)
    {
        ProfileFunction Importer3mf importer{doc.getSharedLogger()};
        importer.load(filename, doc);
    }

    void mergeFrom3mfFile(std::filesystem::path filename, Document & doc)
    {
        ProfileFunction Importer3mf importer{doc.getSharedLogger()};
        importer.merge(filename, doc);
    }
} // namespace gladius::io