#include "OptimizeOutputs.h"

#include "Document.h"
#include "Model.h"

namespace gladius::nodes
{
    OptimizeOutputs::OptimizeOutputs(Assembly * assembly)
        : m_assembly(assembly)
    {
    }

    void OptimizeOutputs::optimize()
    {
        if (m_assembly == nullptr)
        {
            throw std::runtime_error("Assembly is null");
        }

        for (auto & [modelId, model] : m_assembly->getFunctions())
        {
            if (!model)
            {
                continue;
            }
            markFunctionOutputsAsUnused(*model);
        }

        for (auto & [modelId, model] : m_assembly->getFunctions())
        {
            markUsedOutputs(*model);
            propagateUsedFunctionOutputs(*model);
        }
    }

    void OptimizeOutputs::markFunctionOutputsAsUnused(Model & model)
    {
        auto * endNode = model.getEndNode();
        if (endNode == nullptr)
        {
            throw std::runtime_error("End node is null");
        }
        for (auto & parameter : endNode->parameter())
        {
            parameter.second.setConsumedByFunction(false);
        }
    }

    class MarkUsedOutputsVisitor : public Visitor
    {
        void visit(NodeBase & baseNode)
        {
            for (auto & [parameterName, parameter] : baseNode.parameter())
            {
                auto & source = parameter.getSource();
                if (source.has_value() && source.value().port)
                {
                    source.value().port->setIsUsed(true);
                }
            }
        }
    };

    void OptimizeOutputs::markUsedOutputs(Model & model)
    {
        MarkUsedOutputsVisitor visitor;
        model.visitNodes(visitor);
    }

    void OptimizeOutputs::propagateUsedFunctionOutputs(gladius::nodes::Model & model)
    {
        if (m_assembly == nullptr)
        {
            return;
        }

        // Find all function call nodes, and use their outputs to mark the corresponding
        // output ports as used.

        auto functionCallVisitor = OnTypeVisitor<FunctionCall>(
          [&](FunctionCall & functionCallNode)
          {
              auto const funcId = functionCallNode.getFunctionId();
              auto func = m_assembly->findModel(funcId);
              if (!func)
              {
                  throw std::runtime_error(fmt::format("Function {} not found", funcId));
              }

              auto const & funcOutputs = functionCallNode.getOutputs();
              auto * endNode = func->getEndNode();
              if (!endNode)
              {
                  throw std::runtime_error("End node is null");
              }

              for (auto & [outputName, output] : funcOutputs)
              {
                  // find the corresponding parameter in the end node
                  auto & endNodeParams = endNode->parameter();
                  auto iter = endNodeParams.find(outputName);
                  if (iter == endNodeParams.end())
                  {
                      continue;
                  }

                  // mark the parameter as used, if the function output is used
                  bool const isUsed = output.isUsed();
                  iter->second.setConsumedByFunction(isUsed);
              }
          });

        model.visitNodes(functionCallVisitor);
    }
} // namespace gladius::nodes