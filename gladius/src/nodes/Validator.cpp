
#include "Validator.h"

namespace gladius::nodes
{
    bool Validator::validate(Assembly & assembly)
    {
        m_errors.clear();
        for (auto & [name, function] : assembly.getFunctions())
        {
            validateModel(*function);
        }
        return m_errors.empty();
    }

    ValidationErrors const & Validator::getErrors() const
    {
        return m_errors;
    }

    void Validator::validateModel(Model & model)
    {
        model.updateGraphAndOrderIfNeeded();
        model.updateTypes();

        auto const & graph = model.getGraph();
        bool const isCyclic = nodes::graph::isCyclic(graph);

        if (isCyclic)
        {
            m_errors.push_back(
              {"Cyclic graph detected", model.getDisplayName().value_or("unknown"), "", "", ""});
        }

        for (auto & [nodeId, node] : model)
        {
            validateNode(*node, model);
        }
    }

    void Validator::validateNode(NodeBase & node, Model & model)
    {
        for (auto & [parameterName, parameter] : node.parameter())
        {
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
                    continue;
                }

                if (parameter.getTypeIndex() != referendedPort->getTypeIndex())
                {
                    m_errors.push_back(
                      ValidationError{"Parameter type does not match port type",  // message
                                      model.getDisplayName().value_or("unknown"), // model
                                      node.getDisplayName(),                      // node
                                      referendedPort->getUniqueName(),            // port
                                      parameterName});                            // parameter
                    parameter.setValid(false);
                }
            }
        }
    }
}
