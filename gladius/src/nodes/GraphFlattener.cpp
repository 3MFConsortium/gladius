#include "GraphFlattener.h"
#include "Profiling.h"
#include "io/3mf/ResourceDependencyGraph.h"
#include <fmt/format.h>

namespace gladius::nodes
{
    GraphFlattener::GraphFlattener(Assembly const & assembly)
        : m_assembly(assembly), m_dependencyGraph(nullptr), m_hasDependencyGraph(false)
    {
    }
    
    GraphFlattener::GraphFlattener(Assembly const & assembly, io::ResourceDependencyGraph const * dependencyGraph)
        : m_assembly(assembly), m_dependencyGraph(dependencyGraph), m_hasDependencyGraph(dependencyGraph != nullptr)
    {
    }

    // Integrate one functionCall into the target model
    void GraphFlattener::integrateFunctionCall(nodes::FunctionCall const & functionCall,
                                               Model & target)
    {
        ProfileFunction;
        // 1. Find the referenced model
        auto functionId = functionCall.getFunctionId();
        
        // Quick check for used functions first - this is the most frequently hit early-out condition
        if (m_usedFunctions.find(functionId) == m_usedFunctions.end())
        {
            return; // Skip function integration immediately if not in used functions set
        }

        // Check if any outputs are actually used
        bool isFunctionOutputUsed = false;
        for (auto const & output : functionCall.getOutputs())
        {
            if (output.second.isUsed())
            {
                isFunctionOutputUsed = true;
                break;
            }
        }

        if (!isFunctionOutputUsed)
        {
            return;
        }

        // Cache the referenced function to avoid multiple lookups
        auto referencedFunction = m_assembly.findModel(functionId);
        if (!referencedFunction)
        {
            throw std::runtime_error(
              fmt::format("Referenced function {} not found", functionCall.getFunctionId()));
        }

        // Check for self-reference (circular dependency)
        ResourceId refResourceId = referencedFunction->getResourceId();
        if (refResourceId == target.getResourceId())
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

        // Reserve capacity for used functions based on the number of models in assembly
        m_usedFunctions.reserve(m_assembly.getFunctions().size());
        
        // First find all functions that are actually used - will use dependency graph if available
        findUsedFunctions();

        // The assembly model is always used
        m_usedFunctions.insert(modelToFlat->getResourceId());

        // Flatten recursively starting with the top-level model
        flattenRecursive(*modelToFlat);

        // Clean up after flattening
        deleteFunctions();
        deleteFunctionCallNodes();
        
        // Update the graph order
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

        // Ensure recursive flattening is done before integrating
        flattenRecursive(model);

        // Pre-allocate collections to avoid reallocations
        std::unordered_map<std::string, std::string> nameMapping; // old name -> new name
        nameMapping.reserve(100); // Reserve space for a reasonable number of nodes

        std::vector<NodeBase *> createdNodes;
        createdNodes.reserve(100); // Reserve space for a reasonable number of nodes

        // 1. Integrate all nodes first to create the new structure
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

        // Cache Begin node for input mapping
        auto const * beginNode = model.getBeginNode();

        // 3. Update the source ports for each new node
        for (auto * node : createdNodes)
        {
            for (auto & [parameterName, parameter] : node->parameter())
            {
                auto & source = parameter.getSource();
                if (!source.has_value() || !source.value().port)
                {
                    continue; // Skip if no source or port
                }

                auto & originalSourcePort = source.value().port;
                auto const * originalSourceNode = originalSourcePort->getParent();
                
                if (!originalSourceNode)
                {
                    throw std::runtime_error("Source node not found");
                }
                
                auto const & originalSourceNodeName = originalSourceNode->getUniqueName();
                auto & sourcePortName = originalSourcePort->getShortName();

                // If originalSourceNode is a BeginNode, we need to find the corresponding
                // inputs to the function call
                if (dynamic_cast<Begin const *>(originalSourceNode))
                {
                    auto const & inputs = functionCall.constParameter();
                    auto inputIt = inputs.find(sourcePortName);
                    
                    if (inputIt == inputs.end())
                    {
                        throw std::runtime_error(
                          fmt::format("Input {} not found", sourcePortName));
                    }
                    
                    auto const & input = inputIt->second;
                    auto const & inputSource = input.getConstSource();
                    if (!inputSource.has_value() || !inputSource.value().port)
                    {
                        throw std::runtime_error(
                          fmt::format("Input {} has no valid source", sourcePortName));
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

                // Fast lookup in the name mapping for regular nodes
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

                parameter.setInputFromPort(*newSourcePort);
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
        // Find all usages of the outputs of the function call and set
        // the corresponding outputs from the end node of the function

        auto const & outputs = functionCall.getOutputs();
        if (outputs.empty())
        {
            return; // Early exit if no outputs
        }
        
        // Cache the end node for repeated lookups
        auto const & endNode = model.getEndNode();
        auto const & functionCallId = functionCall.getId();
        
        // Create a lookup set for fast output name checks
        std::unordered_set<std::string> outputNames;
        outputNames.reserve(outputs.size());
        for (auto const & output : outputs)
        {
            outputNames.insert(output.first);
        }

        auto const & inputParameters = target.getParameterRegistry();
        for (auto const & [parameterId, input] : inputParameters)
        {
            auto const & inputSource = input->getSource();
            if (!inputSource.has_value() || !inputSource.value().port)
            {
                continue;
            }

            auto const & inputSourcePort = inputSource.value().port;
            auto const & inputSourcePortName = inputSourcePort->getShortName();
            
            // Skip if not an output we care about
            if (outputNames.find(inputSourcePortName) == outputNames.end())
            {
                continue;
            }

            auto const * inputSourceNode = inputSourcePort->getParent();
            if (!inputSourceNode || inputSourceNode->getId() != functionCallId)
            {
                continue;
            }

            // We found a parameter that uses this function call output
            auto * const parameter = endNode->getParameter(inputSourcePortName);
            if (!parameter)
            {
                throw std::runtime_error(
                  fmt::format("Output {} not found in end node", inputSourcePortName));
            }

            auto const & sourceInOriginalModel = parameter->getSource();
            if (!sourceInOriginalModel.has_value())
            {
                throw std::runtime_error(fmt::format("Parameter {} of node {} has no source",
                                                     inputSourcePortName,
                                                     endNode->getUniqueName()));
            }

            auto parentNodeInOriginalModel =
              model.getNode(sourceInOriginalModel.value().nodeId);
            if (!parentNodeInOriginalModel.has_value() || !parentNodeInOriginalModel.value())
            {
                throw std::runtime_error(
                  fmt::format("Parent node of output {} not found", inputSourcePortName));
            }

            auto const & parentNodeNameInOriginalModel =
              parentNodeInOriginalModel.value()->getUniqueName();
            auto parentNodeNameInTargetModelIter =
              nameMapping.find(parentNodeNameInOriginalModel);
            if (parentNodeNameInTargetModelIter == std::end(nameMapping))
            {
                throw std::runtime_error(
                  fmt::format("Parent node of output {} not found", inputSourcePortName));
            }

            auto * parentNodeInTargetModel =
              target.findNode(parentNodeNameInTargetModelIter->second);
            if (!parentNodeInTargetModel)
            {
                throw std::runtime_error(
                  fmt::format("Parent node of output {} not found", inputSourcePortName));
            }

            auto * outputPortInTargetModel =
              parentNodeInTargetModel->findOutputPort(sourceInOriginalModel.value().shortName);

            if (!outputPortInTargetModel)
            {
                throw std::runtime_error(fmt::format("Output port {} not found", inputSourcePortName));
            }

            input->setInputFromPort(*outputPortInTargetModel);
        }
    }

    void GraphFlattener::findUsedFunctions()
    {
        ProfileFunction;
        m_usedFunctions.clear();

        auto modelToFlat = m_assembly.assemblyModel();
        if (!modelToFlat)
        {
            throw std::runtime_error("Assembly model not found");
        }
        
        // If we have a dependency graph, use it for optimized lookup
        if (m_hasDependencyGraph && m_dependencyGraph)
        {
            findUsedFunctionsUsingDependencyGraph(*modelToFlat);
        }
        else
        {
            findUsedFunctionsInModel(*modelToFlat);
        }
    }

    void GraphFlattener::findUsedFunctionsInModel(Model & model)
    {
        ProfileFunction;
        
        // Find all function calls in the model with used outputs
        auto functionCallVisitor = OnTypeVisitor<FunctionCall>(
            [&](FunctionCall & functionCallNode)
            {
                // Check if any output is used before proceeding
                bool isFunctionOutputUsed = false;
                for (auto const & output : functionCallNode.getOutputs())
                {
                    if (output.second.isUsed())
                    {
                        isFunctionOutputUsed = true;
                        break;
                    }
                }

                if (!isFunctionOutputUsed)
                {
                    return; // Skip if no outputs are used
                }
                    
                auto functionId = functionCallNode.getFunctionId();
                
                // Check if function is already processed
                if (m_usedFunctions.find(functionId) != m_usedFunctions.end())
                {
                    return; // Already processed this function
                }
                
                auto referencedFunction = m_assembly.findModel(functionId);
                if (!referencedFunction)
                {
                    return; // Function not found
                }
                
                // Mark this function as used
                m_usedFunctions.insert(functionId);
                
                // Recursively find used functions in this function
                findUsedFunctionsInModel(*referencedFunction);
            });
        
        model.visitNodes(functionCallVisitor);
    }

    void GraphFlattener::findUsedFunctionsUsingDependencyGraph(Model & rootModel)
    {
        ProfileFunction;
        
        // First, add the root model's resource ID to used functions
        ResourceId rootResourceId = rootModel.getResourceId();
        m_usedFunctions.insert(rootResourceId);
        
        // Use direct function calls to identify initial set of functions
        std::vector<ResourceId> functionQueue;
        
        // Find function call nodes in the root model to initialize the queue
        auto functionCallVisitor = OnTypeVisitor<FunctionCall>(
            [&](FunctionCall & functionCallNode)
            {
                // Skip if no outputs are used
                bool isFunctionOutputUsed = false;
                for (auto const & output : functionCallNode.getOutputs())
                {
                    if (output.second.isUsed())
                    {
                        isFunctionOutputUsed = true;
                        break;
                    }
                }
                
                if (!isFunctionOutputUsed)
                    return;
                    
                ResourceId functionId = functionCallNode.getFunctionId();
                if (m_usedFunctions.find(functionId) == m_usedFunctions.end())
                {
                    m_usedFunctions.insert(functionId);
                    functionQueue.push_back(functionId);
                    
                    // If we have a dependency graph, let's use it to find all dependencies up front
                    // This can potentially save multiple traversals if the dependency graph already 
                    // contains complete dependency data
                    if (m_hasDependencyGraph && m_dependencyGraph)
                    {
                        auto resourcePtr = m_dependencyGraph->getResourceById(functionId);
                        if (resourcePtr)
                        {
                            // Get all required resources from the dependency graph
                            auto requiredResources = m_dependencyGraph->getAllRequiredResources(resourcePtr);
                            for (const auto& resource : requiredResources)
                            {
                                auto depId = resource->GetResourceID();
                                if (m_usedFunctions.find(depId) == m_usedFunctions.end())
                                {
                                    m_usedFunctions.insert(depId);
                                    functionQueue.push_back(depId);
                                }
                            }
                        }
                    }
                }
            });
        
        // Use the named visitor
        rootModel.visitNodes(functionCallVisitor);
        
        // Process the queue of functions - for each function, get its model and find more function calls
        while (!functionQueue.empty())
        {
            ResourceId currentFunctionId = functionQueue.back();
            functionQueue.pop_back();
            
            auto currentModel = m_assembly.findModel(currentFunctionId);
            if (!currentModel)
                continue;
                
            // Find function calls in this model - create a named visitor to avoid temporary object issues
            auto nestedFunctionCallVisitor = OnTypeVisitor<FunctionCall>(
                [&](FunctionCall & functionCallNode)
                {
                    // Skip if no outputs are used
                    bool isFunctionOutputUsed = false;
                    for (auto const & output : functionCallNode.getOutputs())
                    {
                        if (output.second.isUsed())
                        {
                            isFunctionOutputUsed = true;
                            break;
                        }
                    }
                    
                    if (!isFunctionOutputUsed)
                        return;
                        
                    ResourceId nestedFunctionId = functionCallNode.getFunctionId();
                    if (m_usedFunctions.find(nestedFunctionId) == m_usedFunctions.end())
                    {
                        m_usedFunctions.insert(nestedFunctionId);
                        functionQueue.push_back(nestedFunctionId);
                        
                        // Use the dependency graph here as well
                        if (m_hasDependencyGraph && m_dependencyGraph)
                        {
                            auto resourcePtr = m_dependencyGraph->getResourceById(nestedFunctionId);
                            if (resourcePtr)
                            {
                                auto requiredResources = m_dependencyGraph->getAllRequiredResources(resourcePtr);
                                for (const auto& resource : requiredResources)
                                {
                                    auto depId = resource->GetResourceID();
                                    if (m_usedFunctions.find(depId) == m_usedFunctions.end())
                                    {
                                        m_usedFunctions.insert(depId);
                                        functionQueue.push_back(depId);
                                    }
                                }
                            }
                        }
                    }
                });
                
            // Now use the named visitor
            currentModel->visitNodes(nestedFunctionCallVisitor);
        }
    }
} // namespace gladius::nodes