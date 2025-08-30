
#include "Validator.h"
#include "DerivedNodes.h"
#include <fmt/format.h>

namespace
{
    using namespace gladius::nodes;

    // Helper to determine if a node should be exempt from input-connection validation
    inline bool isNodeExemptFromInputValidation(NodeBase & node)
    {
        // Input/Output markers
        if (dynamic_cast<Begin *>(&node) != nullptr)
            return true;
        if (dynamic_cast<End *>(&node) != nullptr)
            return true;

        // Constant literal providers
        if (dynamic_cast<ConstantScalar *>(&node) != nullptr)
            return true;
        if (dynamic_cast<ConstantVector *>(&node) != nullptr)
            return true;
        if (dynamic_cast<ConstantMatrix *>(&node) != nullptr)
            return true;

        return false;
    }
}

namespace gladius::nodes
{
    bool Validator::validate(Assembly & assembly)
    {
        m_errors.clear();
        for (auto & [name, function] : assembly.getFunctions())
        {
            validateModel(*function, assembly);
        }
        return m_errors.empty();
    }

    ValidationErrors const & Validator::getErrors() const
    {
        return m_errors;
    }

    void Validator::validateModel(Model & model, Assembly & assembly)
    {
        model.updateGraphAndOrderIfNeeded();
        model.updateTypes();

        model.updateValidityState();

        for (auto & [nodeId, node] : model)
        {
            validateNode(*node, model, assembly);
        }
    }

    void Validator::validateNode(NodeBase & node, Model & model, Assembly & assembly)
    {
        validateNodeImpl(node, model);
    }

    void Validator::validateNodeImpl(NodeBase & node, Model & model)
    {
        // Skip validation for special nodes (I/O markers and constants) via type-based checks
        if (isNodeExemptFromInputValidation(node))
        {
            return;
        }

        // Create a descriptive model identifier that includes both name and ID
        auto modelDisplayName = model.getDisplayName().value_or("unknown");
        auto modelId = model.getResourceId();
        std::string modelInfo = fmt::format("{} (ID: {})", modelDisplayName, modelId);

        for (auto & [parameterName, parameter] : node.parameter())
        {
            if (!parameter.getConstSource().has_value() && parameter.isInputSourceRequired())
            {
                m_errors.push_back(ValidationError{
                  fmt::format(
                    "Node '{}' requires input for parameter '{}' but no connection found. "
                    "Connect an output from another node to this parameter.",
                    node.getDisplayName(),
                    parameterName),
                  modelInfo,
                  node.getDisplayName(),
                  "unknown",
                  parameterName});
                model.setIsValid(false);
            }
            if (parameter.getConstSource().has_value())
            {
                auto const & source = parameter.getConstSource().value();
                auto referendedPort = model.getPort(source.portId);
                parameter.setValid(true);

                if (!referendedPort)
                {
                    m_errors.push_back(ValidationError{
                      fmt::format("Parameter '{}' of node '{}' references a non-existing port. "
                                  "The referenced node or port may have been deleted.",
                                  parameterName,
                                  node.getDisplayName()), // message
                      modelInfo,                          // model
                      node.getDisplayName(),              // node
                      "unknown",                          // port
                      parameterName});                    // parameter

                    parameter.setValid(false);
                    model.setIsValid(false);
                    continue;
                }

                if (parameter.getTypeIndex() != referendedPort->getTypeIndex())
                {
                    m_errors.push_back(ValidationError{
                      fmt::format(
                        "Type mismatch: Parameter '{}' of node '{}' expects different data type "
                        "than provided by connected port '{}'. Check node documentation for "
                        "required types.",
                        parameterName,
                        node.getDisplayName(),
                        referendedPort->getUniqueName()), // message
                      modelInfo,                          // model
                      node.getDisplayName(),              // node
                      referendedPort->getUniqueName(),    // port
                      parameterName});                    // parameter
                    parameter.setValid(false);
                    model.setIsValid(false);
                }
            }
        }
    }

    void Validator::validateNode(FunctionCall & node, Model & model, Assembly & assembly)
    {
        validateNodeImpl(node, model);
        node.resolveFunctionId();
        auto referencedId = node.getFunctionId();
        auto referencedModel = assembly.findModel(referencedId);

        // Create a descriptive model identifier that includes both name and ID
        auto modelDisplayName = model.getDisplayName().value_or("unknown");
        auto modelId = model.getResourceId();
        std::string modelInfo = fmt::format("{} (ID: {})", modelDisplayName, modelId);

        if (!referencedModel)
        {
            m_errors.push_back(ValidationError{"Function reference not found",
                                               modelInfo,
                                               node.getDisplayName(),
                                               "unknown",
                                               "FunctionId"});
            model.setIsValid(false);
        }
    }
}
