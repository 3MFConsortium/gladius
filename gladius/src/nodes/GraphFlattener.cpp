#include "GraphFlattener.h"
#include "Profiling.h"
#include <fmt/format.h>

namespace gladius::nodes
{
    GraphFlattener::GraphFlattener(Assembly const & assembly)
        : m_assembly(assembly)
    {
    }

    // Integrate one functionCall into the target model
    void GraphFlattener::integrateFunctionCall(nodes::FunctionCall const & functionCall,
                                               Model & target)
    {
        ProfileFunction;
        // 1. Find the referenced model
        auto functionId = functionCall.getFunctionId();

        bool isFunctionOutputUsed = false;
        for (auto const & output : functionCall.getOutputs())
        {
            auto const & outputName = output.first;
            auto const & outputValue = output.second;
            if (outputValue.isUsed())
            {
                isFunctionOutputUsed = true;
                break;
            }
        }

        if (!isFunctionOutputUsed)
        {
            return;
        }

        auto referencedFunction = m_assembly.findModel(functionCall.getFunctionId());
        if (!referencedFunction)
        {
            throw std::runtime_error(
              fmt::format("Referenced function {} not found", functionCall.getFunctionId()));
        }

        if (referencedFunction->getResourceId() == target.getResourceId())
        {
            throw std::runtime_error(fmt::format(
              "Function {} references itself", referencedFunction->getDisplayName().value_or("")));
        }

        if (m_flatteningDepth > 100)
        {
            throw std::runtime_error(
              fmt::format("Flattening depth of {} exceeded",
                          referencedFunction->getDisplayName().value_or("")));
        }

        // 2. Integrate the referenced model into the target model
        m_flatteningDepth++;
        integrateModel(*referencedFunction, target, functionCall);
        m_flatteningDepth--;
    }

    Assembly GraphFlattener::flatten()
    {
        ProfileFunction;
        auto modelToFlat = m_assembly.assemblyModel();

        if (!modelToFlat)
        {
            throw std::runtime_error("Assembly model not found");
        }

        flattenRecursive(*modelToFlat);

        deleteFunctions();

        deleteFunctionCallNodes();
        m_assembly.assemblyModel()->updateGraphAndOrderIfNeeded();
        return m_assembly;
    }

    void GraphFlattener::flattenRecursive(Model & model)
    {
        ProfileFunction;
        // 1. Find all function calls
        auto functionCallVisitor = OnTypeVisitor<FunctionCall>(
          [&](FunctionCall & functionCallNode)
          {
              functionCallNode.resolveFunctionId();
              if (!functionCallNode.updateTypes(model))
              {
                  throw std::runtime_error(fmt::format("Function call {} has invalid types",
                                                       functionCallNode.getUniqueName()));
              }
              integrateFunctionCall(functionCallNode, model);
          });
        model.visitNodes(functionCallVisitor);
    }

    void GraphFlattener::deleteFunctions()
    {
        ProfileFunction;
        auto modelToFlat = m_assembly.assemblyModel();

        if (!modelToFlat)
        {
            throw std::runtime_error("Assembly model not found");
        }

        // remove all submodels besides the assembly model
        std::vector<ResourceId> modelsToDelete;
        for (auto & [id, model] : m_assembly.getFunctions())
        {
            if (id != modelToFlat->getResourceId())
            {
                modelsToDelete.push_back(id);
            }
        }

        for (const auto & id : modelsToDelete)
        {
            m_assembly.deleteModel(id);
        }
    }

    void GraphFlattener::deleteFunctionCallNodes()
    {
        ProfileFunction;
        auto modelToFlat = m_assembly.assemblyModel();

        if (!modelToFlat)
        {
            throw std::runtime_error("Assembly model not found");
        }

        // remove all function call nodes
        auto functionCallVisitor = OnTypeVisitor<FunctionCall>(
          [&](FunctionCall & functionCallNode)
          { modelToFlat->removeNodeWithoutLinks(functionCallNode.getId()); });
        modelToFlat->visitNodes(functionCallVisitor);
    }

    /**
     * Integrate a model into another model by cloning and updating the nodes and connections.
     *
     * @param model The model to be integrated.
     * @param target The target model to integrate into.
     * @param functionCall The function call node that triggers the integration.
     * @throws std::runtime_error if integration fails or required nodes/ports are not found.
     */
    void GraphFlattener::integrateModel(Model & model,
                                        Model & target,
                                        nodes::FunctionCall const & functionCall)
    {
        ProfileFunction;
        if (model.getResourceId() == target.getResourceId())
        {
            return; // nothing to integrate
        }

        if (!m_assembly.findModel(model.getResourceId()))
        {
            throw std::runtime_error(fmt::format("Model {} with id {} not found",
                                                 model.getDisplayName().value_or(""),
                                                 model.getResourceId()));
        }

        // check, if all required inputs are connected
        auto const & inputs = functionCall.constParameter();
        for (auto const & [inputName, input] : inputs)
        {
            if (inputName == nodes::FieldNames::FunctionId)
            {
                continue;
            }
            auto const & inputSource = input.getConstSource();
            if (!inputSource.has_value())
            {
                throw std::runtime_error(fmt::format("Input {} of function call {} has no source",
                                                     inputName,
                                                     functionCall.getUniqueName()));
            }

            if (!inputSource.value().port)
            {
                throw std::runtime_error(fmt::format("Input {} of function call {} has no port",
                                                     inputName,
                                                     functionCall.getUniqueName()));
            }
        }

        flattenRecursive(model);

        std::unordered_map<std::string, std::string> nameMapping; // old name -> new name

        std::vector<NodeBase *> createdNodes;
        // 1. Integrate all nodes
        for (auto & node : model)
        {
            // Skip Begin and End nodes
            if (dynamic_cast<Begin const *>(node.second.get()) ||
                dynamic_cast<End const *>(node.second.get()))
            {
                continue;
            }

            auto clonedNode = node.second->clone();
            auto * integratedNode = target.insert(std::move(clonedNode));
            if (!integratedNode)
            {
                throw std::runtime_error(
                  fmt::format("Could not integrate node {}", node.second->getUniqueName()));
            }
            createdNodes.push_back(integratedNode);

            // 2. Update the name mapping
            nameMapping[node.second->getUniqueName()] = integratedNode->getUniqueName();
        }

        // 3. Update the source ports
        for (auto * node : createdNodes)
        {
            for (auto & [parameterName, parameter] : node->parameter())
            {
                auto & source = parameter.getSource();
                if (source.has_value() && source.value().port)
                {

                    auto & originalSourcePort = source.value().port;
                    auto const * originalSourceNode = originalSourcePort->getParent();
                    auto const & originalSourceNodeName = originalSourceNode->getUniqueName();
                    auto & sourcePortName = originalSourcePort->getShortName();

                    // If originalSourceNode is a BeginNode, we need to find the corresponding
                    // inputs to the function call

                    if (auto * beginNode = dynamic_cast<Begin const *>(originalSourceNode))
                    {
                        auto const & inputs = functionCall.constParameter();
                        auto const & input = inputs.at(sourcePortName);
                        auto const & inputSource = input.getConstSource();
                        if (!inputSource.has_value())
                        {
                            throw std::runtime_error(
                              fmt::format("Input {} has no source", sourcePortName));
                        }

                        if (!inputSource.value().port)
                        {
                            throw std::runtime_error(
                              fmt::format("Input {} has no port", sourcePortName));
                        }

                        auto portId = inputSource.value().portId;

                        auto * port = target.getPort(portId);
                        if (!port)
                        {
                            throw std::runtime_error(fmt::format("Port {} not found", portId));
                        }

                        parameter.setInputFromPort(*port);
                        continue;
                    }

                    auto newSourceNodeNameIter = nameMapping.find(originalSourceNodeName);
                    if (newSourceNodeNameIter == std::end(nameMapping))
                    {
                        throw std::runtime_error(
                          fmt::format("Source node {} not found", originalSourceNodeName));
                    }

                    auto * newSourceNode = target.findNode(newSourceNodeNameIter->second);
                    if (!newSourceNode)
                    {
                        throw std::runtime_error(
                          fmt::format("Source node {} not found", newSourceNodeNameIter->second));
                    }

                    Port * newSourcePort = newSourceNode->findOutputPort(sourcePortName);

                    if (!newSourcePort)
                    {
                        throw std::runtime_error(
                          fmt::format("Source port {} not found", sourcePortName));
                    }

                    if (newSourcePort->getShortName() != sourcePortName)
                    {
                        throw std::runtime_error(
                          fmt::format("Source port {} not found", sourcePortName));
                    }

                    parameter.setInputFromPort(*newSourcePort);
                }
            }
        }

        // 4. Update the outputs
        rerouteOutputs(model, target, functionCall, nameMapping);
    }

    void
    GraphFlattener::rerouteOutputs(Model & model,
                                   Model & target,
                                   nodes::FunctionCall const & functionCall,
                                   std::unordered_map<std::string, std::string> const & nameMapping)
    {
        ProfileFunction;
        // Find all usages of the outputs of the function call and and set
        // the corresponding outputs from the end node of the function

        auto const & outputs = functionCall.getOutputs();
        for (auto const & [outputName, output] : outputs)
        {
            auto const & inputParameters = target.getParameterRegistry();
            for (auto const & [parameterId, input] : inputParameters)
            {
                auto const & inputSource = input->getSource();
                if (!inputSource.has_value())
                {
                    continue;
                }

                auto const & inputSourcePort = inputSource.value().port;
                if (!inputSourcePort)
                {
                    continue;
                }

                auto const & inputSourcePortName = inputSourcePort->getShortName();
                if (inputSourcePortName != outputName)
                {
                    continue;
                }

                auto const * inputSourceNode = inputSourcePort->getParent();
                if (!inputSourceNode)
                {
                    throw std::runtime_error(
                      fmt::format("Input source node not found for {}", parameterId));
                }

                if (inputSourceNode->getId() != functionCall.getId())
                {
                    continue;
                }

                auto const & endNode = model.getEndNode();
                auto * const parameter = endNode->getParameter(outputName);
                if (!parameter)
                {
                    throw std::runtime_error(
                      fmt::format("Output {} not found in end node", outputName));
                }

                auto const & sourceInOriginalModel = parameter->getSource();
                if (!sourceInOriginalModel.has_value())
                {
                    throw std::runtime_error(fmt::format("Parameter {} of node {} has no source",
                                                         outputName,
                                                         endNode->getUniqueName()));
                }

                auto parentNodeInOriginalModel =
                  model.getNode(sourceInOriginalModel.value().nodeId);
                if (!parentNodeInOriginalModel.has_value() || !parentNodeInOriginalModel.value())
                {
                    throw std::runtime_error(
                      fmt::format("Parent node of output {} not found", outputName));
                }

                auto const & parentNodeNameInOriginalModel =
                  parentNodeInOriginalModel.value()->getUniqueName();
                auto parentNodeNameInTargetModelIter =
                  nameMapping.find(parentNodeNameInOriginalModel);
                if (parentNodeNameInTargetModelIter == std::end(nameMapping))
                {
                    throw std::runtime_error(
                      fmt::format("Parent node of output {} not found", outputName));
                }

                auto * parentNodeInTargetModel =
                  target.findNode(parentNodeNameInTargetModelIter->second);
                if (!parentNodeInTargetModel)
                {
                    throw std::runtime_error(
                      fmt::format("Parent node of output {} not found", outputName));
                }

                auto * outputPortInTargetModel =
                  parentNodeInTargetModel->findOutputPort(sourceInOriginalModel.value().shortName);

                if (!outputPortInTargetModel)
                {
                    throw std::runtime_error(fmt::format("Output port {} not found", outputName));
                }

                input->setInputFromPort(*outputPortInTargetModel);
            }
        }
    }
} // namespace gladius::nodes