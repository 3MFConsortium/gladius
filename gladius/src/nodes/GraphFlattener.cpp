#include "GraphFlattener.h"
#include "Profiling.h"
#include "io/3mf/ResourceDependencyGraph.h"
#include <fmt/format.h>

namespace gladius::nodes
{
    GraphFlattener::GraphFlattener(Assembly const & assembly)
        : m_assembly(assembly)
        , m_dependencyGraph(nullptr)
        , m_hasDependencyGraph(false)
    {
    }

    GraphFlattener::GraphFlattener(Assembly const & assembly,
                                   io::ResourceDependencyGraph const * dependencyGraph)
        : m_assembly(assembly)
        , m_dependencyGraph(dependencyGraph)
        , m_hasDependencyGraph(dependencyGraph != nullptr)
    {
    }

    // Integrate one functionCall into the target model
    void GraphFlattener::integrateFunctionCall(nodes::FunctionCall const & functionCall,
                                               Model & target)
    {
        ProfileFunction;
        // 1. Find the referenced model
        auto functionId = functionCall.getFunctionId();

        // Quick check for used functions first - this is the most frequently hit early-out
        // condition
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

        // Check if this function call has already been integrated
        if (m_integratedFunctionCalls.find(&functionCall) != m_integratedFunctionCalls.end())
        {
            m_redundantIntegrationSkips++;
            fmt::print("Function call {} already integrated, skipping (total skips: {}).\n",
                       functionCall.getDisplayName(),
                       m_redundantIntegrationSkips);
            return; // Skip if this function call has already been integrated
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
        fmt::print("Integrating function call: {} into model: {} (depth: {})\n",
                   functionCall.getDisplayName(),
                   target.getResourceId(),
                   m_flatteningDepth);

        // Add to integrated set before proceeding with integration
        m_integratedFunctionCalls.insert(&functionCall);

        integrateModel(*referencedFunction, target, functionCall);
        m_flatteningDepth--;
    }

    Assembly GraphFlattener::flatten()
    {
        ProfileFunction;
        // print the expected node count
        size_t expectedNodeCount = calculateExpectedNodeCount();
        fmt::print("Expected node count after flattening: {}\n", expectedNodeCount);
        auto modelToFlat = m_assembly.assemblyModel();

        if (!modelToFlat)
        {
            throw std::runtime_error("Assembly model not found");
        }

        // Clear set of integrated function calls and reset statistics
        m_integratedFunctionCalls.clear();
        m_redundantIntegrationSkips = 0;

        // Reserve capacity for used functions based on the number of models in assembly
        m_usedFunctions.reserve(m_assembly.getFunctions().size());

        // First find all functions that are actually used - will use dependency graph if available
        findUsedFunctions();

        // The assembly model is always used
        m_usedFunctions.insert(modelToFlat->getResourceId());

        // Simplify all used models before flattening
        simplifyUsedModels();

        // Flatten recursively starting with the top-level model
        flattenRecursive(*modelToFlat);

        // Clean up after flattening
        deleteFunctions();
        deleteFunctionCallNodes();

        // Update the graph order
        m_assembly.assemblyModel()->updateGraphAndOrderIfNeeded();

        // actual node count
        size_t actualNodeCount = m_assembly.assemblyModel()->getSize();
        fmt::print("Actual node count after flattening: {}\n", actualNodeCount);
        if (actualNodeCount != expectedNodeCount)
        {
            fmt::print("Warning: Expected node count ({}) does not match actual node count ({}).\n",
                       expectedNodeCount,
                       actualNodeCount);
        }

        // Print integration statistics
        fmt::print("Integration statistics:\n");
        fmt::print("  - Integrated function calls: {}\n", m_integratedFunctionCalls.size());
        fmt::print("  - Redundant integration attempts avoided: {}\n", m_redundantIntegrationSkips);

        return m_assembly;
    }

    /**
     * @brief Calculate the expected number of nodes after flattening without performing the actual
     * flattening
     *
     * This method simulates the flattening process to count how many nodes would be in the final
     * flattened model without actually modifying any models. It takes into account:
     * - Only functions with used outputs
     * - Skipping Begin and End nodes during integration
     * - Each node being integrated exactly once (no duplicates)
     *
     * @return size_t The expected number of nodes in the flattened model
     * @throws std::runtime_error if the assembly model is not found
     */
    size_t GraphFlattener::calculateExpectedNodeCount()
    {
        ProfileFunction;
        auto modelToFlat = m_assembly.assemblyModel();

        if (!modelToFlat)
        {
            throw std::runtime_error("Assembly model not found");
        }

        // Clear and reserve capacity for used functions
        m_usedFunctions.clear();
        m_usedFunctions.reserve(m_assembly.getFunctions().size());

        // Clear set of integrated function calls
        m_integratedFunctionCalls.clear();

        // Find all functions that are actually used
        findUsedFunctions();

        // The assembly model is always used
        m_usedFunctions.insert(modelToFlat->getResourceId());

        // Keep track of node count without Begin/End nodes
        size_t totalNodeCount = 0;

        // Track models that have been counted to avoid double-counting
        std::unordered_set<ResourceId> countedModels;

        // Count all nodes in the used functions, excluding Begin and End nodes
        for (auto const & functionId : m_usedFunctions)
        {
            // Skip if already counted
            if (countedModels.find(functionId) != countedModels.end())
            {
                continue;
            }

            auto model = m_assembly.findModel(functionId);
            if (!model)
            {
                continue;
            }

            // Count nodes in this model (excluding Begin and End nodes)
            for (auto & node : *model)
            {
                // Skip Begin and End nodes as they're not integrated
                if (dynamic_cast<Begin const *>(node.second.get()) ||
                    dynamic_cast<End const *>(node.second.get()))
                {
                    continue;
                }

                // Skip FunctionCall nodes as they'll be replaced
                if (dynamic_cast<FunctionCall const *>(node.second.get()))
                {
                    continue;
                }

                totalNodeCount++;
            }

            // Mark as counted
            countedModels.insert(functionId);

            // Count nodes from function calls within this model
            countNodesFromFunctionCalls(*model, countedModels, totalNodeCount);
        }

        return totalNodeCount;
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
    /**
     * @brief Validates that all required inputs of a function call are properly connected
     *
     * @param functionCall The function call to validate inputs for
     * @throws std::runtime_error if any input is not properly connected
     */
    void GraphFlattener::validateFunctionCallInputs(nodes::FunctionCall const & functionCall) const
    {
        ProfileFunction;
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
    }

    /**
     * @brief Integrates nodes from source model into target model and creates a name mapping
     *
     * @param model Source model containing nodes to integrate
     * @param target Target model to integrate nodes into
     * @param nameMapping Output parameter that will map source node names to target node names
     * @return std::vector<NodeBase *> Collection of newly created nodes
     * @throws std::runtime_error if node integration fails
     */
    std::vector<NodeBase *> GraphFlattener::integrateNodesFromModel(
      Model & model,
      Model & target,
      std::unordered_map<std::string, std::string> & nameMapping)
    {
        ProfileFunction;
        // Pre-allocate to avoid reallocations
        std::vector<NodeBase *> createdNodes;
        createdNodes.reserve(100);

        // Integrate all nodes first to create the new structure
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

            // Update the name mapping
            nameMapping[node.second->getUniqueName()] = integratedNode->getUniqueName();
        }

        // print size of created nodes and size of target model
        std::cout << "Created nodes: " << createdNodes.size() << std::endl;
        std::cout << "Target model size: " << target.getSize() << std::endl;

        return createdNodes;
    }

    /**
     * @brief Updates connections for newly integrated nodes
     *
     * @param model Source model
     * @param target Target model
     * @param functionCall Function call that triggered the integration
     * @param nameMapping Mapping from source node names to target node names
     * @param createdNodes Collection of newly created nodes
     * @throws std::runtime_error if connection update fails due to missing nodes or ports
     */
    void GraphFlattener::updateNodeConnections(
      Model & model,
      Model & target,
      nodes::FunctionCall const & functionCall,
      std::unordered_map<std::string, std::string> const & nameMapping,
      std::vector<NodeBase *> const & createdNodes)
    {
        ProfileFunction;
        // Cache Begin node for input mapping
        auto const * beginNode = model.getBeginNode();

        // Update the source ports for each new node
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
                    connectBeginNodeInput(target, functionCall, parameter, sourcePortName);
                    continue;
                }

                connectRegularNodeInput(
                  target, parameter, originalSourceNodeName, sourcePortName, nameMapping);
            }
        }
    }

    /**
     * @brief Connects an input parameter to a corresponding function call input
     *
     * @param target Target model
     * @param functionCall Function call that contains the input
     * @param parameter Parameter to connect
     * @param sourcePortName Source port name to look for in function call inputs
     * @throws std::runtime_error if connection fails due to missing input or port
     */
    void GraphFlattener::connectBeginNodeInput(Model & target,
                                               nodes::FunctionCall const & functionCall,
                                               VariantParameter & parameter,
                                               std::string const & sourcePortName)
    {
        ProfileFunction;
        auto const & inputs = functionCall.constParameter();
        auto inputIt = inputs.find(sourcePortName);

        if (inputIt == inputs.end())
        {
            throw std::runtime_error(fmt::format("Input {} not found", sourcePortName));
        }

        auto const & input = inputIt->second;
        auto const & inputSource = input.getConstSource();
        if (!inputSource.has_value() || !inputSource.value().port)
        {
            throw std::runtime_error(fmt::format("Input {} has no valid source", sourcePortName));
        }

        auto portId = inputSource.value().portId;
        auto * port = target.getPort(portId);
        if (!port)
        {
            throw std::runtime_error(fmt::format("Port {} not found", portId));
        }

        parameter.setInputFromPort(*port);
    }

    /**
     * @brief Connects an input parameter to a corresponding port in a regular node
     *
     * @param target Target model
     * @param parameter Parameter to connect
     * @param originalSourceNodeName Original source node name
     * @param sourcePortName Source port name
     * @param nameMapping Mapping from source node names to target node names
     * @throws std::runtime_error if connection fails due to missing node or port
     */
    void GraphFlattener::connectRegularNodeInput(
      Model & target,
      VariantParameter & parameter,
      std::string const & originalSourceNodeName,
      std::string const & sourcePortName,
      std::unordered_map<std::string, std::string> const & nameMapping)
    {
        ProfileFunction;
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
            throw std::runtime_error(fmt::format("Source port {} not found", sourcePortName));
        }

        parameter.setInputFromPort(*newSourcePort);
    }

    /**
     * @brief Integrates a model into another model by cloning and updating the nodes and
     * connections
     *
     * @param model The model to be integrated
     * @param target The target model to integrate into
     * @param functionCall The function call node that triggers the integration
     * @throws std::runtime_error if integration fails or required nodes/ports are not found
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

        // Validate inputs
        validateFunctionCallInputs(functionCall);

        // Ensure recursive flattening is done before integrating
        flattenRecursive(model);

        // Pre-allocate name mapping
        std::unordered_map<std::string, std::string> nameMapping; // old name -> new name
        nameMapping.reserve(100);

        // Integrate nodes and get created node references
        auto createdNodes = integrateNodesFromModel(model, target, nameMapping);

        // Update connections for the new nodes
        updateNodeConnections(model, target, functionCall, nameMapping, createdNodes);

        // Update the outputs
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

            auto parentNodeInOriginalModel = model.getNode(sourceInOriginalModel.value().nodeId);
            if (!parentNodeInOriginalModel.has_value() || !parentNodeInOriginalModel.value())
            {
                throw std::runtime_error(
                  fmt::format("Parent node of output {} not found", inputSourcePortName));
            }

            auto const & parentNodeNameInOriginalModel =
              parentNodeInOriginalModel.value()->getUniqueName();
            auto parentNodeNameInTargetModelIter = nameMapping.find(parentNodeNameInOriginalModel);
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
                throw std::runtime_error(
                  fmt::format("Output port {} not found", inputSourcePortName));
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
                          auto requiredResources =
                            m_dependencyGraph->getAllRequiredResources(resourcePtr);
                          for (const auto & resource : requiredResources)
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

        // Process the queue of functions - for each function, get its model and find more function
        // calls
        while (!functionQueue.empty())
        {
            ResourceId currentFunctionId = functionQueue.back();
            functionQueue.pop_back();

            auto currentModel = m_assembly.findModel(currentFunctionId);
            if (!currentModel)
                continue;

            // Find function calls in this model - create a named visitor to avoid temporary object
            // issues
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
                              auto requiredResources =
                                m_dependencyGraph->getAllRequiredResources(resourcePtr);
                              for (const auto & resource : requiredResources)
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

    void GraphFlattener::simplifyUsedModels()
    {
        ProfileFunction;

        // Process all models that were identified as used
        for (const auto & modelId : m_usedFunctions)
        {
            auto model = m_assembly.findModel(modelId);
            if (model)
            {
                // Apply model simplification
                model->simplifyModel();
            }
        }
    }

    /**
     * @brief Counts nodes that would be added from function calls in a model
     *
     * This method recursively counts nodes from function calls within the given model.
     * It checks if any outputs of function calls are used and counts nodes from the
     * referenced function if they haven't been counted already.
     *
     * @param model The model to examine function calls in
     * @param countedModels Set of models that have already been counted
     * @param totalNodeCount Reference to running total of node count
     */
    void GraphFlattener::countNodesFromFunctionCalls(Model const & model,
                                                     std::unordered_set<ResourceId> & countedModels,
                                                     size_t & totalNodeCount)
    {
        ProfileFunction;

        // Create a visitor for function calls
        auto functionCallVisitor = OnTypeVisitor<FunctionCall>(
          [&](FunctionCall & functionCallNode)
          {
              // Check if any output is actually used
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

              // Get the function ID
              auto functionId = functionCallNode.getFunctionId();

              // Check if already counted
              if (countedModels.find(functionId) != countedModels.end())
              {
                  return; // Already counted this function
              }

              // Check if function exists
              auto referencedFunction = m_assembly.findModel(functionId);
              if (!referencedFunction)
              {
                  return; // Function not found
              }

              // Check for self-reference (circular dependency)
              if (referencedFunction->getResourceId() == model.getResourceId())
              {
                  return; // Avoid infinite recursion
              }

              // Count nodes in this referenced function (excluding Begin and End nodes)
              for (auto & node : *referencedFunction)
              {
                  // Skip Begin, End, and FunctionCall nodes
                  if (dynamic_cast<Begin const *>(node.second.get()) ||
                      dynamic_cast<End const *>(node.second.get()) ||
                      dynamic_cast<FunctionCall const *>(node.second.get()))
                  {
                      continue;
                  }

                  totalNodeCount++;
              }

              // Mark as counted
              countedModels.insert(functionId);

              // Recursively count nodes from function calls in the referenced function
              countNodesFromFunctionCalls(*referencedFunction, countedModels, totalNodeCount);
          });

        // Apply the visitor to the model
        const_cast<Model &>(model).visitNodes(functionCallVisitor);
    }
} // namespace gladius::nodes