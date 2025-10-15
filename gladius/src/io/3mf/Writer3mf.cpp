#include "Writer3mf.h"

#include "Document.h"
#include "EventLogger.h"
#include "ResourceKey.h"
#include "nodes/Assembly.h"
#include "nodes/Model.h"
#include "nodes/Parameter.h"
#include "nodes/Visitor.h"

#include <lib3mf_abi.hpp>
#include <lib3mf_implicit.hpp>
#include <lib3mf_types.hpp>

#include <fmt/format.h>
#include <unordered_map>

namespace gladius::io
{
    class NodeTypeMap
    {
      public:
        NodeTypeMap()
            : m_typeMap{
                {typeid(nodes::Addition), Lib3MF::eImplicitNodeType::Addition},
                {typeid(nodes::Subtraction), Lib3MF::eImplicitNodeType::Subtraction},
                {typeid(nodes::Multiplication), Lib3MF::eImplicitNodeType::Multiplication},
                {typeid(nodes::Division), Lib3MF::eImplicitNodeType::Division},
                {typeid(nodes::ConstantScalar), Lib3MF::eImplicitNodeType::Constant},
                {typeid(nodes::ConstantVector), Lib3MF::eImplicitNodeType::ConstVec},
                {typeid(nodes::ConstantMatrix), Lib3MF::eImplicitNodeType::ConstMat},
                {typeid(nodes::ComposeVector), Lib3MF::eImplicitNodeType::ComposeVector},
                {typeid(nodes::DecomposeVector), Lib3MF::eImplicitNodeType::DecomposeVector},
                {typeid(nodes::ComposeMatrix), Lib3MF::eImplicitNodeType::ComposeMatrix},
                {typeid(nodes::ComposeMatrixFromColumns),
                 Lib3MF::eImplicitNodeType::MatrixFromColumns},
                {typeid(nodes::ComposeMatrixFromRows), Lib3MF::eImplicitNodeType::MatrixFromRows},
                {typeid(nodes::DotProduct), Lib3MF::eImplicitNodeType::Dot},
                {typeid(nodes::CrossProduct), Lib3MF::eImplicitNodeType::Cross},
                {typeid(nodes::MatrixVectorMultiplication),
                 Lib3MF::eImplicitNodeType::MatVecMultiplication},
                {typeid(nodes::Transpose), Lib3MF::eImplicitNodeType::Transpose},
                {typeid(nodes::Inverse), Lib3MF::eImplicitNodeType::Inverse},
                {typeid(nodes::Sine), Lib3MF::eImplicitNodeType::Sinus},
                {typeid(nodes::Cosine), Lib3MF::eImplicitNodeType::Cosinus},
                {typeid(nodes::Tangent), Lib3MF::eImplicitNodeType::Tan},
                {typeid(nodes::ArcSin), Lib3MF::eImplicitNodeType::ArcSin},
                {typeid(nodes::ArcCos), Lib3MF::eImplicitNodeType::ArcCos},
                {typeid(nodes::ArcTan), Lib3MF::eImplicitNodeType::ArcTan},
                {typeid(nodes::ArcTan2), Lib3MF::eImplicitNodeType::ArcTan2},
                {typeid(nodes::Min), Lib3MF::eImplicitNodeType::Min},
                {typeid(nodes::Max), Lib3MF::eImplicitNodeType::Max},
                {typeid(nodes::Abs), Lib3MF::eImplicitNodeType::Abs},
                {typeid(nodes::Fmod), Lib3MF::eImplicitNodeType::Fmod},
                {typeid(nodes::Pow), Lib3MF::eImplicitNodeType::Pow},
                {typeid(nodes::Sqrt), Lib3MF::eImplicitNodeType::Sqrt},
                {typeid(nodes::Exp), Lib3MF::eImplicitNodeType::Exp},
                {typeid(nodes::Log), Lib3MF::eImplicitNodeType::Log},
                {typeid(nodes::Log2), Lib3MF::eImplicitNodeType::Log2},
                {typeid(nodes::Log10), Lib3MF::eImplicitNodeType::Log10},
                {typeid(nodes::Select), Lib3MF::eImplicitNodeType::Select},
                {typeid(nodes::Clamp), Lib3MF::eImplicitNodeType::Clamp},
                {typeid(nodes::SinH), Lib3MF::eImplicitNodeType::Sinh},
                {typeid(nodes::CosH), Lib3MF::eImplicitNodeType::Cosh},
                {typeid(nodes::TanH), Lib3MF::eImplicitNodeType::Tanh},
                {typeid(nodes::Round), Lib3MF::eImplicitNodeType::Round},
                {typeid(nodes::Ceil), Lib3MF::eImplicitNodeType::Ceil},
                {typeid(nodes::Floor), Lib3MF::eImplicitNodeType::Floor},
                {typeid(nodes::Sign), Lib3MF::eImplicitNodeType::Sign},
                {typeid(nodes::Fract), Lib3MF::eImplicitNodeType::Fract},
                {typeid(nodes::FunctionCall), Lib3MF::eImplicitNodeType::FunctionCall},
                {typeid(nodes::SignedDistanceToMesh), Lib3MF::eImplicitNodeType::Mesh},
                {typeid(nodes::Length), Lib3MF::eImplicitNodeType::Length},
                {typeid(nodes::Resource), Lib3MF::eImplicitNodeType::ConstResourceID},
                {typeid(nodes::VectorFromScalar), Lib3MF::eImplicitNodeType::VectorFromScalar},
                {typeid(nodes::UnsignedDistanceToMesh), Lib3MF::eImplicitNodeType::UnsignedMesh},
                {typeid(nodes::Mod), Lib3MF::eImplicitNodeType::Mod}}
        {
        }

        Lib3MF::eImplicitNodeType getType(nodes::NodeBase & node) const
        {
            auto it = m_typeMap.find(typeid(node));
            if (it != m_typeMap.end())
            {
                return it->second;
            }
            else
            {
                throw std::runtime_error(
                  fmt::format("Unknown node type of node {}", node.getUniqueName()));
            }
        }

      private:
        // Mapping between node types and Lib3MF::eImplicitNodeType
        std::unordered_map<std::type_index, Lib3MF::eImplicitNodeType> m_typeMap;

        // Helper function to get the Lib3MF::eImplicitNodeType for a given node type
    };

    Lib3MF::eImplicitPortType convertPortType(std::type_index typeIndex)
    {
        if (typeIndex == typeid(float))
        {
            return Lib3MF::eImplicitPortType::Scalar;
        }
        else if (typeIndex == typeid(gladius::nodes::float3))
        {
            return Lib3MF::eImplicitPortType::Vector;
        }
        else if (typeIndex == typeid(gladius::nodes::Matrix4x4))
        {
            return Lib3MF::eImplicitPortType::Matrix;
        }
        else if ((typeIndex == gladius::nodes::ParameterTypeIndex::ResourceId) ||
                 (typeIndex == typeid(int)))
        {
            return Lib3MF::eImplicitPortType::ResourceID;
        }
        else
        {
            throw std::runtime_error("Unknown type index");
        }
    }

    Lib3MF::eImplicitNodeConfiguration convertToNodeConfiguration(nodes::RuleType ruleType)
    {
        switch (ruleType)
        {
        case nodes::RuleType::Default:
            return Lib3MF::eImplicitNodeConfiguration::Default;
        case nodes::RuleType::Scalar:
            return Lib3MF::eImplicitNodeConfiguration::ScalarToScalar;
        case nodes::RuleType::Vector:
            return Lib3MF::eImplicitNodeConfiguration::VectorToVector;
        case nodes::RuleType::Matrix:
            return Lib3MF::eImplicitNodeConfiguration::MatrixToMatrix;
        default:
            throw std::runtime_error("Unknown rule type");
        }
    }

    Lib3MF::PResource findResourceByModelResourceId(Lib3MF::PModel model3mf, Lib3MF_uint32 id)
    {
        auto resIter = model3mf->GetResources();
        while (resIter->MoveNext())
        {
            auto resource = resIter->GetCurrent();
            if (resource->GetModelResourceID() == id)
            {
                return resource;
            }
        }
        return {};
    }

    Lib3MF::sVector convertVector3(gladius::nodes::float3 const & vec)
    {
        return {vec.x, vec.y, vec.z};
    }

    Lib3MF::sMatrix4x4 convertMatrix4x4(gladius::nodes::Matrix4x4 const & mat)
    {
        Lib3MF::sMatrix4x4 mat3mf;

        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                mat3mf.m_Field[row][col] = mat[row][col];
            }
        }
        return mat3mf;
    }

    class NodeCreator : public nodes::Visitor
    {
      public:
        explicit NodeCreator(Lib3MF::PImplicitFunction targetFunc, Lib3MF::PModel model)
            : m_targetFunc(targetFunc)
            , m_model(model)
        {
        }

        void setTargetFunction(Lib3MF::PImplicitFunction targetFunc)
        {
            m_targetFunc = targetFunc;
        }

        void visit(nodes::Begin &) override
        {
            // Do nothing
        }

        void visit(nodes::End &) override
        {
            // Do nothing
        }

        void visit(nodes::ConstantScalar & node) override
        {
            if (!m_targetFunc)
            {
                throw std::runtime_error("No target function set");
            }

            auto node3mf = m_targetFunc->AddConstantNode(
              node.getUniqueName(), node.getDisplayName(), node.getTag());

            initNode(node, node3mf);
            node3mf->SetConstant(node.getValue());
        }

        void visit(nodes::ConstantVector & node) override
        {
            if (!m_targetFunc)
            {
                throw std::runtime_error("No target function set");
            }

            auto node3mf = m_targetFunc->AddConstVecNode(
              node.getUniqueName(), node.getDisplayName(), node.getTag());

            initNode(node, node3mf);
            node3mf->SetVector(convertVector3(node.getValue()));
        }

        void visit(nodes::ConstantMatrix & node) override
        {
            if (!m_targetFunc)
            {
                throw std::runtime_error("No target function set");
            }

            auto node3mf = m_targetFunc->AddConstMatNode(
              node.getUniqueName(), node.getDisplayName(), node.getTag());

            initNode(node, node3mf);
            node3mf->SetMatrix(convertMatrix4x4(node.getValue()));
        }

        void visit(nodes::Resource & node) override
        {
            if (!m_targetFunc)
            {
                throw std::runtime_error("No target function set");
            }

            if (!m_model)
            {
                throw std::runtime_error("No model set");
            }

            auto node3mf = m_targetFunc->AddResourceIdNode(
              node.getUniqueName(), node.getDisplayName(), node.getTag());

            initNode(node, node3mf);

            auto resource = findResourceByModelResourceId(m_model, node.getResourceId());

            if (!resource)
            {
                throw std::runtime_error(
                  fmt::format("Could not find resource with id {}", node.getResourceId()));
            }

            node3mf->SetResource(resource);
        }

        void visit(nodes::FunctionCall & node) override
        {
            if (!m_targetFunc)
            {
                throw std::runtime_error("No target function set");
            }

            if (!m_model)
            {
                throw std::runtime_error("No model set");
            }

            auto functioncallNode = m_targetFunc->AddFunctionCallNode(
              node.getUniqueName(), node.getDisplayName(), node.getTag());

            initNode(node, functioncallNode);

            auto & funcIdParam = node.parameter().at(nodes::FieldNames::FunctionId);
            if (!funcIdParam.getSource().has_value())
            {
                // create an additional resource id node for the function id in the 3mf model
                auto resIdNode = m_targetFunc->AddResourceIdNode(
                  fmt::format("{}_{}", node.getUniqueName(), nodes::FieldNames::FunctionId),
                  fmt::format("{}_{}", node.getDisplayName(), nodes::FieldNames::FunctionId),
                  node.getTag());

                node.resolveFunctionId();
                auto resource = findResourceByModelResourceId(m_model, node.getFunctionId());

                if (!resource)
                {
                    throw std::runtime_error(
                      fmt::format("Could not find resource with id {}", node.getFunctionId()));
                }

                resIdNode->SetResource(resource);

                // link
                auto resIdOutput = resIdNode->GetOutputValue();
                auto funcIdInput = functioncallNode->GetInputFunctionID();

                m_targetFunc->AddLink(resIdOutput, funcIdInput);
            }
        }

        void visit(nodes::NodeBase & node) override
        {
            createNode(node);
        }

      private:
        Lib3MF::PImplicitFunction m_targetFunc;
        Lib3MF::PModel m_model;

        NodeTypeMap const m_typeMap;

        void initNode(nodes::NodeBase & node, Lib3MF::PImplicitNode node3mf)
        {
            for (auto & [portName, input] : node.parameter())
            {
                auto input3mf = node3mf->FindInput(portName);
                if (!input3mf)
                {
                    input3mf = node3mf->AddInput(portName, portName);
                }
                if (!input3mf)
                {
                    throw std::runtime_error(fmt::format(
                      "Could not add input {} to node {}", portName, node.getUniqueName()));
                }
                try
                {
                    input3mf->SetType(convertPortType(input.getTypeIndex()));
                }
                catch (std::exception & e)
                {
                    throw std::runtime_error(
                      fmt::format("Could not set type of input {} of node {}: {}\t typeindex:{}",
                                  portName,
                                  node.getUniqueName(),
                                  e.what(),
                                  input.getTypeIndex().name()));
                }
            }

            for (auto & [portName, output] : node.getOutputs())
            {
                auto output3mf = node3mf->FindOutput(portName);
                if (!output3mf)
                {
                    output3mf = node3mf->AddOutput(portName, portName);
                }
                if (!output3mf)
                {
                    throw std::runtime_error(fmt::format(
                      "Could not add output {} to node {}", portName, node.getUniqueName()));
                }
                output3mf->SetType(convertPortType(output.getTypeIndex()));
            }
        }

        Lib3MF::PImplicitNode createNode(nodes::NodeBase & node)
        {
            if (!m_targetFunc)
            {
                throw std::runtime_error("No target function set");
            }

            auto node3mf = m_targetFunc->AddNode(m_typeMap.getType(node),
                                                 node.getUniqueName(),
                                                 convertToNodeConfiguration(node.getRuleType()),
                                                 node.getDisplayName(),
                                                 node.getTag());

            initNode(node, node3mf);
            return node3mf;
        }
    };

    class LinkCreator : public nodes::Visitor
    {
      public:
        explicit LinkCreator(Lib3MF::PImplicitFunction targetFunc)
            : m_targetFunc(targetFunc)
        {
        }

        void setTargetFunction(Lib3MF::PImplicitFunction targetFunc)
        {
            m_targetFunc = targetFunc;
        }

        void visit(nodes::Begin &) override
        {
            // Do nothing
        }

        void visit(nodes::End &) override
        {
            // Do nothing
        }

        void visit(nodes::NodeBase & node) override
        {
            if (!m_targetFunc)
            {
                throw std::runtime_error("No target function set");
            }

            auto node3mf = findNode(node.getUniqueName());
            if (!node3mf)
            {
                throw std::runtime_error(
                  fmt::format("Could not find node {}", node.getUniqueName()));
            }

            for (auto & [portName, input] : node.parameter())
            {
                if (!input.getSource())
                {
                    continue;
                }
                auto input3mf = node3mf->FindInput(portName);
                if (!input3mf)
                {
                    throw std::runtime_error(fmt::format(
                      "Could not find input {} of node {}", portName, node.getUniqueName()));
                }

                auto const sourcePath =
                  m_currentModel->getSourceName(input.getSource().value().portId);
                auto const targetPath = fmt::format("{}.{}", node.getUniqueName(), portName);
                m_targetFunc->AddLinkByNames(sourcePath, targetPath);
            }
        }

      private:
        Lib3MF::PImplicitFunction m_targetFunc;

        Lib3MF::PImplicitNode findNode(std::string const & identifier)
        {
            auto nodeIter = m_targetFunc->GetNodes();
            while (nodeIter->MoveNext())
            {
                auto node = nodeIter->GetCurrent();
                if (node->GetIdentifier() == identifier)
                {
                    return node;
                }
            }

            return {};
        }
    };

    Lib3MF::PImplicitFunction findExistingFunction(Lib3MF::PModel model3mf,
                                                   gladius::nodes::Model const & function)
    {
        auto resIter = model3mf->GetResources();

        auto const resId = function.getResourceId();
        while (resIter->MoveNext())
        {
            Lib3MF::PResource existingFunction = resIter->GetCurrent();
            auto const existingResId = existingFunction->GetModelResourceID();
            if (existingResId == resId)
            {
                auto function3mf =
                  std::dynamic_pointer_cast<Lib3MF::CImplicitFunction>(existingFunction);

                if (function3mf)
                {
                    return function3mf;
                }
            }
        }
        return {};
    }

    Writer3mf::Writer3mf(events::SharedLogger logger)
        : Writer3mfBase(std::move(logger))
    {
    }

    void Writer3mf::save(std::filesystem::path const & filename,
                         Document const & doc,
                         bool writeThumbnail)
    {
        m_logger->logInfo(fmt::format("Starting save operation to file: {}", filename.string()));
        auto model = doc.get3mfModel();

        if (!model)
        {
            m_logger->addEvent({"No 3MF model to save.", events::Severity::Error});
            return;
        }
        m_logger->logInfo("3MF model found successfully");
        auto metaDataGroup = model->GetMetaDataGroup();

        if (!metaDataGroup)
        {
            m_logger->addEvent({"No metadata group found.", events::Severity::Warning});
            return;
        }
        m_logger->logInfo("Metadata group retrieved successfully");

        try
        {
            auto constexpr metaKeyApp = "Application";
            auto metaApplication = metaDataGroup->GetMetaDataByKey("", metaKeyApp);
            if (!metaApplication)
            {
                metaDataGroup->AddMetaData("", metaKeyApp, "Gladius", "string", true);
                m_logger->logInfo("Added Gladius metadata successfully");
            }
            else
            {
                m_logger->logInfo("Gladius metadata already exists");
            }
        }
        catch (const std::exception & e)
        {
            m_logger->addEvent(
              {fmt::format("Failed to add metadata: {}", e.what()), events::Severity::Warning});
        }

        m_logger->logInfo("Starting model update");
        updateModel(doc);
        m_logger->logInfo("Model update completed");

        if (writeThumbnail)
        {
            try
            {
                m_logger->logInfo("Starting thumbnail update");
                updateThumbnail(const_cast<Document &>(doc), model);
                m_logger->logInfo("Thumbnail update completed");
            }
            catch (const std::exception & e)
            {
                m_logger->addEvent(
                  {fmt::format("Failed to update thumbnail. Writing file without thumbnail: {}",
                               e.what()),
                   events::Severity::Warning});
            }
        }

        try
        {
            m_logger->logInfo("Creating 3MF writer");
            auto writer = model->QueryWriter("3mf");
            m_logger->logInfo(fmt::format("Writing to file: {}", filename.string()));
            writer->WriteToFile(filename.string().c_str());
            m_logger->logInfo("File write completed successfully");
        }
        catch (Lib3MF::ELib3MFException const & e)
        {
            m_logger->addEvent(
              {fmt::format("3MF write error: {} (Error Code: {})", e.what(), e.getErrorCode()),
               events::Severity::Error});
        }
        catch (const std::exception & e)
        {
            m_logger->addEvent({fmt::format("Standard exception during write: {}", e.what()),
                                events::Severity::Error});
        }
    }

    void Writer3mf::updateModel(Document const & doc)
    {
        m_logger->logInfo("Starting updateModel operation");
        auto model3mf = doc.get3mfModel();

        if (!model3mf)
        {
            m_logger->addEvent({"No 3MF model to update.", events::Severity::Error});
            return;
        }

        m_logger->logInfo("Retrieved 3MF model for update");
        auto assembly = doc.getAssembly();
        if (!assembly)
        {
            m_logger->addEvent({"No assembly found in document.", events::Severity::Error});
            return;
        }

        auto functions = assembly->getFunctions();
        m_logger->logInfo(fmt::format("Found {} functions to process", functions.size()));

        for (auto & [name, model] : functions)
        {
            m_logger->logInfo(fmt::format("Processing function: {}", name));

            if (model->isManaged())
            {
                m_logger->logInfo(fmt::format("Skipping managed function: {}", name));
                continue; // dont write functions that represent other aspects the 3mf model
            }

            // Detect unsupported FunctionGradient nodes and skip serialization for such models
            // until 3MF specification & lib3mf support is available.
            // TODO(FunctionGradient-3MF): Add full serialization support for FunctionGradient
            // nodes once the 3MF implicit function extension defines a corresponding node type.
            struct FunctionGradientDetector : nodes::Visitor
            {
                bool found = false;
                void visit(nodes::FunctionGradient & /*fg*/) override
                {
                    found = true;
                }

                void visit(nodes::NodeBase & /*nb*/) override
                {
                    return; // Do nothing for other nodes
                }
                // Inherit other visits from base (which will traverse children via
                // Model::visitNodes)
            } detector;

            model->visitNodes(detector);
            if (detector.found)
            {
                m_logger->addEvent(
                  {fmt::format(
                     "Skipping function '{}' during 3MF export: contains FunctionGradient node(s) "
                     "which are not yet supported in 3MF serialization.",
                     name),
                   events::Severity::Warning});
                continue; // Skip this function to avoid runtime error in NodeTypeMap
            }

            try
            {
                auto function3mf = findExistingFunction(model3mf, *model);

                if (function3mf)
                {
                    m_logger->logInfo(fmt::format("Updating existing function: {}", name));
                    function3mf->Clear();
                    fillFunction(function3mf, *model, model3mf);
                    m_logger->logInfo(fmt::format("Successfully updated function: {}", name));
                }
                else
                {
                    m_logger->logInfo(fmt::format("Adding new function: {}", name));
                    addFunctionTo3mf(*model, model3mf);
                    m_logger->logInfo(fmt::format("Successfully added new function: {}", name));
                }
            }
            catch (const std::exception & e)
            {
                m_logger->addEvent(
                  {fmt::format("Failed to process function '{}': {}", name, e.what()),
                   events::Severity::Error});
            }
        }
        m_logger->logInfo("updateModel operation completed");
    }

    void Writer3mf::addFunctionTo3mf(gladius::nodes::Model & model, Lib3MF::PModel model3mf)
    {
        m_logger->logInfo(
          fmt::format("Creating new implicit function for model: {}", model.getModelName()));
        try
        {
            auto newFunction = model3mf->AddImplicitFunction();
            m_logger->logInfo("Implicit function created, filling with data");
            fillFunction(newFunction, model, model3mf);
            m_logger->logInfo("Function data filled successfully");
        }
        catch (const std::exception & e)
        {
            m_logger->addEvent({fmt::format("Failed to add function to 3MF: {}", e.what()),
                                events::Severity::Error});
            throw;
        }
    }

    void Writer3mf::fillFunction(Lib3MF::PImplicitFunction function,
                                 gladius::nodes::Model & model,
                                 Lib3MF::PModel model3mf)
    {
        m_logger->logInfo(fmt::format("Starting fillFunction for model: {}", model.getModelName()));
        try
        {
            NodeCreator nodeCreator{function, model3mf};
            LinkCreator linkCreator{function};

            if (model.getDisplayName().has_value())
            {
                function->SetDisplayName(model.getDisplayName().value());
                m_logger->logInfo(
                  fmt::format("Set display name: {}", model.getDisplayName().value()));
            }

            auto inputs = model.getInputs();
            m_logger->logInfo(fmt::format("Adding {} input ports", inputs.size()));
            for (auto & [portName, input] : inputs)
            {
                function->AddInput(
                  portName, input.getShortName(), convertPortType(input.getTypeIndex()));
                m_logger->logInfo(fmt::format("Added input port: {}", portName));
            }

            m_logger->logInfo("Starting node visitation");
            model.visitNodes(nodeCreator);
            model.visitNodes(linkCreator);
            m_logger->logInfo("Node visitation completed");

            auto outputs = model.getOutputs();
            m_logger->logInfo(fmt::format("Processing {} outputs", outputs.size()));
            for (auto & [portName, output] : outputs)
            {
                if (!output.getSource().has_value())
                {
                    continue;
                }
                auto output3mf =
                  function->AddOutput(portName, portName, convertPortType(output.getTypeIndex()));
                m_logger->logInfo(fmt::format("Added output port: {}", portName));

                auto const sourcePath = model.getSourceName(output.getSource().value().portId);
                auto const targetPath = fmt::format("outputs.{}", portName);
                function->AddLinkByNames(sourcePath, targetPath);
                m_logger->logInfo(fmt::format("Linked output: {} -> {}", sourcePath, targetPath));
            }

            m_logger->logInfo("fillFunction completed successfully");
        }
        catch (const std::exception & e)
        {
            m_logger->addEvent(
              {fmt::format("Failed to fill function: {}", e.what()), events::Severity::Error});
            throw;
        }
    }

    void Writer3mf::saveFunction(std::filesystem::path const & filename,
                                 gladius::nodes::Model & function)
    {
        auto model3mf = m_wrapper->CreateModel();
        auto newFunction = model3mf->AddImplicitFunction();
        fillFunction(newFunction, function, model3mf);

        auto writer = model3mf->QueryWriter("3mf");
        writer->WriteToFile(filename.string().c_str());
    }

    void
    saveTo3mfFile(std::filesystem::path const & filename, Document const & doc, bool writeThumbnail)
    {
        Writer3mf writer(doc.getSharedLogger());
        writer.save(filename, doc, writeThumbnail);
    }

    void saveFunctionTo3mfFile(std::filesystem::path const & filename,
                               gladius::nodes::Model & function)
    {
        Writer3mf writer{{}};
        writer.saveFunction(filename, function);
    }

} // namespace gladius::io