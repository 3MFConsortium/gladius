#include "Importer3mf.h"

#include <lib3mf/Cpp/lib3mf_abi.hpp>
#include <lib3mf/Cpp/lib3mf_implicit.hpp>
#include <lib3mf/Cpp/lib3mf_types.hpp>

#include "Builder.h"
#include "Document.h"
#include "ImageExtractor.h"
#include "Parameter.h"
#include "Profiling.h"
#include "VdbImporter.h"
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
            std::cout << e.what() << std::endl;
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
            return VariantParameter(Matrix4x4(), ContentType::Transformation);
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
                  fmt::format("Could not cast node {} to ResourceIdNode", node->getUniqueName()));
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
        }
    }

    void Importer3mf::loadComponentObjects(Lib3MF::PModel model, Document & doc)
    {
        ProfileFunction auto objectIterator = model->GetObjects();
        nodes::Builder builder;
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

                builder.addCompositeModel(doc, object->GetResourceID(), components);
            }
        }
    }

    void Importer3mf::loadMeshIfNecessary(Lib3MF::PModel model,
                                          Lib3MF::PMeshObject meshObject,
                                          Document & doc)
    {
        ProfileFunction auto key = ResourceKey(static_cast<int>(meshObject->GetModelResourceID()));
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
            return;
        }
        doc.getGeneratorContext().resourceManager.addResource(key, std::move(mesh));
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

            auto coordinateSystemPort =
              builder.addTransformationToInputCs(*doc.getAssembly()->assemblyModel(), trafo);

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

            auto buildItemCoordinateSystemPort =
              builder.addTransformationToInputCs(*doc.getAssembly()->assemblyModel(), trafo);

            auto levelSetTransform = levelSet->GetTransform();
            auto levelSetTrafo = matrix4x4From3mfTransform(levelSetTransform);

            auto levelSetCoordinateSystemPort = builder.insertTransformation(
              *doc.getAssembly()->assemblyModel(), buildItemCoordinateSystemPort, levelSetTrafo);

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
            if (levelSet->GetMeshBBoxOnly())
            {
                auto const bbox = computeBoundingBox(mesh);
                builder.addBoundingBox(
                  *doc.getAssembly()->assemblyModel(), bbox, buildItemCoordinateSystemPort);
            }
            else
            {

                auto referencedMeshKey = ResourceKey(mesh->GetModelResourceID());
                loadMeshIfNecessary(model, mesh, doc);
                builder.addResourceRef(*doc.getAssembly()->assemblyModel(),
                                       referencedMeshKey,
                                       buildItemCoordinateSystemPort);
            }

            builder.appendIntersectionWithFunction(*doc.getAssembly()->assemblyModel(),
                                                   *gladiusFunction,
                                                   levelSetCoordinateSystemPort,
                                                   channelName);

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
        if (!extractor.open(filename))
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
                if (resMan.hasResource(ResourceKey{image3d->GetModelResourceID()}))
                {
                    continue;
                }

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

                    auto key = ResourceKey{image3d->GetModelResourceID()};
                    key.setDisplayName(image3d->GetName());
                    if (useVdb)
                    {
                        auto grid = extractor.loadAsVdbGrid(fileList);

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
                        componentTrafo = matrix4x4From3mfTransform(component->GetTransform());
                    }

                    buildItemIter->addComponent(
                      {componentObj->GetModelResourceID(), componentTrafo});
                    auto key = ResourceKey{componentObj->GetModelResourceID()};
                    key.setDisplayName(componentObj->GetName());

                    createObject(*componentObj, model, key, componentTrafo, doc);
                }
            }
            else
            {
                auto key = ResourceKey{objectRes->GetModelResourceID()};
                key.setDisplayName(currentBuildItem->GetObjectResource()->GetName());
                createObject(*objectRes, model, key, trafo, doc);
            }
        }
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
            addMeshObject(model, key, mesh, trafo, doc);
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
        // loadComponentObjects(model, doc);
        loadBuildItems(model, doc);
    }

    void Importer3mf::merge(std::filesystem::path const & filename, Document & doc)
    {
        ProfileFunction auto model3mf = doc.get3mfModel();
        if (!model3mf)
        {
            load(filename, doc);
            return;
        }

        auto const modelToMerge = m_wrapper->CreateModel();
        auto const reader = modelToMerge->QueryReader("3mf");

        reader->SetStrictModeActive(true);
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
            model3mf->MergeFromModel(modelToMerge.get());

            loadImageStacks(filename, model3mf, doc);
           loadImplicitFunctions(model3mf, doc);
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
}
