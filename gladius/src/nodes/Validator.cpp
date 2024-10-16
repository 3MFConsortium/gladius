
#include "Validator.h"

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
        model.setIsValid(true);

        auto const & graph = model.getGraph();
        bool const isCyclic = nodes::graph::isCyclic(graph);

        if (isCyclic)
        {
            m_errors.push_back(
              {"Cyclic graph detected", model.getDisplayName().value_or("unknown"), "", "", ""});
            model.setIsValid(false);
        }

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
         for (auto & [parameterName, parameter] : node.parameter())
        {
            if (!parameter.getConstSource().has_value() && parameter.isInputSourceRequired())
            {
                m_errors.push_back(ValidationError{"Missing input",
                                                   model.getDisplayName().value_or("unknown"),
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
                    m_errors.push_back(
                      ValidationError{"Parameter references non-existing port",   // message
                                      model.getDisplayName().value_or("unknown"), // model
                                      node.getDisplayName(),                      // node
                                      "unknown",                                  // port
                                      parameterName});                            // parameter

                    parameter.setValid(false);
                    model.setIsValid(false);
                    continue;
                }

                if (parameter.getTypeIndex() != referendedPort->getTypeIndex())
                {
                    m_errors.push_back(
                      ValidationError{"Datatype mismatch",  // message
                                      model.getDisplayName().value_or("unknown"), // model
                                      node.getDisplayName(),                      // node
                                      referendedPort->getUniqueName(),            // port
                                      parameterName});                            // parameter
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
        if (!referencedModel)
        {
            m_errors.push_back(ValidationError{"Function reference not found",
                                               model.getDisplayName().value_or("unknown"),
                                               node.getDisplayName(),
                                               "unknown",
                                               "FunctionId"});
            model.setIsValid(false);
        }
        

    }
}
