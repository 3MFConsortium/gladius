#include "Assembly.h"
#include "Model.h"
#include "Visitor.h"
#include <cstdio>
#include <cstring>
#include <fmt/format.h>

namespace gladius::nodes
{
    Assembly::Assembly()
    {
        m_subModels[m_assemblyModelId] = std::make_shared<Model>();
    }

    auto Assembly::getFunctions() -> Models &
    {
        return m_subModels;
    }

    auto Assembly::assemblyModel() -> SharedModel &
    {
        return m_subModels.at(m_assemblyModelId);
    }

    void Assembly::deleteModel(ResourceId id)
    {

        auto modelToDeleteIter = m_subModels.find(id);
        if (modelToDeleteIter == std::end(m_subModels))
        {
            return;
        }
        m_subModels.erase(modelToDeleteIter);
    }

    bool Assembly::equals(Assembly const & other)
    {
        if (this->m_subModels.size() != other.m_subModels.size())
        {
            return false;
        }

        for (auto & subModel : m_subModels)
        {
            auto otherIter = other.m_subModels.find(subModel.first);
            if (otherIter == std::end(other.m_subModels))
            {
                return false;
            }
            auto & otherSubModel = otherIter->second;

            if (subModel.second->getGraph().getSize() != otherSubModel->getGraph().getSize())
            {
                return false;
            }
            if (subModel.second->getGraph().getVertices() !=
                otherSubModel->getGraph().getVertices())
            {
                return false;
            }

            auto nodes = subModel.second;
            for (auto & [nodeId, node] : *nodes)
            {
                auto otherNode = otherSubModel->getNode(nodeId);
                if (!otherNode.has_value())
                {
                    return false;
                }

                if ((node->screenPos().x != otherNode.value()->screenPos().x) ||
                    (node->screenPos().y != otherNode.value()->screenPos().y))
                {
                    return false;
                }
                for (auto & [parameterName, parameter] : node->parameter())
                {
                    auto const otherParameterIter =
                      otherNode.value()->parameter().find(parameterName);
                    if (otherParameterIter == std::end(otherNode.value()->parameter()))
                    {
                        return false;
                    }

                    if (parameter.toString() != otherParameterIter->second.toString())
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    Assembly::Assembly(Assembly const & other)
    {
        // copy submodel instances (not the shared_ptr)
        for (auto & [id, model] : other.m_subModels)
        {
            m_subModels[id] = std::make_shared<Model>(*model);
        }

        m_assemblyModelId = other.m_assemblyModelId;
        m_fileName = other.m_fileName;
    }

    void Assembly::visitNodes(Visitor & visitor)
    {
        visitor.setAssembly(this);
        for (auto & model : m_subModels)
        {
            if (!model.second)
            {
                continue;
            }
            model.second->visitNodes(visitor);
        }
    }

    void Assembly::visitAssemblyNodes(Visitor & visitor)
    {
        visitor.setAssembly(this);
        auto model = assemblyModel();
        model->visitNodes(visitor);
    }

    auto Assembly::addModelIfNotExisting(ResourceId id) -> bool
    {
        if (m_subModels.find(id) == std::end(m_subModels))
        {
            m_subModels[id] = std::make_shared<Model>();
            m_subModels[id]->setResourceId(id);
            return true;
        }
        return false;
    }

    bool Assembly::isValid()
    {
        for (auto & model : m_subModels)
        {
            if (!model.second)
            {
                continue;
            }
            if (!model.second->isValid())
            {
                return false;
            }
        }
        return true;
    }

    SharedModel Assembly::findModel(ResourceId id)
    {
        auto modelIter = m_subModels.find(id);
        if (modelIter == std::end(m_subModels))
        {
            return nullptr;
        }
        return modelIter->second;
    }

    void Assembly::updateInputsAndOutputs()
    {
        /*
            1. Loop over all models
            2. Visit all nodes::FunctionCall
            3. Find the referenced model
            4. Update the inputs and outputs
        */

        // for (auto & [id, model] : m_subModels)
        for (auto & elem : m_subModels)
        {
            auto & model = elem.second;
            model->updateTypes();

            auto visitor = OnTypeVisitor<nodes::FunctionCall>(
              [&](auto & functionCall)
              {
                  functionCall.resolveFunctionId();
                  auto referencedId = functionCall.getFunctionId();
                  auto referencedModel = findModel(referencedId);
                  if (!referencedModel)
                  {
                      throw std::runtime_error(fmt::format(
                        "{} references a function with the fucntion id {}, that could not be found",
                        functionCall.getDisplayName(),
                        referencedId));
                  }

                  functionCall.updateInputsAndOutputs(*referencedModel);
                  model->registerInputs(functionCall);
                  model->registerOutputs(functionCall);
              });
            model->visitNodes(visitor);

            model->updateGraphAndOrderIfNeeded();
        }
    }
} // namespace gladius::nodes
