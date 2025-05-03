#pragma once

#include "Assembly.h"
#include <unordered_set>

namespace gladius::io
{
    class ResourceDependencyGraph;
}

namespace gladius::nodes
{
    class GraphFlattener
    {
      public:
        /**
         * @brief Constructs a GraphFlattener with an assembly
         * @param assembly The assembly to flatten
         */
        GraphFlattener(Assembly const & assembly);
        
        /**
         * @brief Constructs a GraphFlattener with an assembly and a resource dependency graph
         * @param assembly The assembly to flatten
         * @param dependencyGraph Pointer to a resource dependency graph for optimized lookups
         */
        GraphFlattener(Assembly const & assembly, 
                      io::ResourceDependencyGraph const * dependencyGraph);

        /**
         * @brief Flatten the graph, so the assembly will only have one single function (model)
         */
        Assembly flatten();
        
        /**
         * @brief Calculate the expected number of nodes after flattening without performing the actual flattening
         * @return The expected node count in the flattened model
         */
        size_t calculateExpectedNodeCount();

      private:
        Assembly m_assembly;
        std::unordered_set<ResourceId> m_usedFunctions;
        io::ResourceDependencyGraph const * m_dependencyGraph = nullptr;
        bool m_hasDependencyGraph = false;

        void findUsedFunctions();
        void findUsedFunctionsInModel(Model & model);
        void findUsedFunctionsUsingDependencyGraph(Model & rootModel);
        void simplifyUsedModels();
        void integrateFunctionCall(nodes::FunctionCall const & functionCall, Model & target);
        void flattenRecursive(Model & model);
        void deleteFunctions();
        void deleteFunctionCallNodes();

        // Integrate one node into the target model
        template <typename NodeType>
        void integrateNode(NodeType & node, Model & target)
        {
            target.insert(node);
        }

        /**
         * @brief Integrate a model into the target model
         * @param model The model to integrate
         * @param target The target model
         * @param functionCall The function call that references the model
         */
        void integrateModel(Model & model, Model & target, nodes::FunctionCall const & functionCall);

        /**
         * @brief Validates that all required inputs of a function call are properly connected
         * @param functionCall The function call to validate inputs for
         */
        void validateFunctionCallInputs(nodes::FunctionCall const & functionCall) const;

        /**
         * @brief Integrates nodes from source model into target model and creates a name mapping
         * @param model Source model containing nodes to integrate
         * @param target Target model to integrate nodes into
         * @param nameMapping Output parameter that will map source node names to target node names
         * @return Collection of newly created nodes
         */
        std::vector<NodeBase *> integrateNodesFromModel(Model & model, 
                                                       Model & target,
                                                       std::unordered_map<std::string, std::string> & nameMapping);

        /**
         * @brief Updates connections for newly integrated nodes
         * @param model Source model 
         * @param target Target model
         * @param functionCall Function call that triggered the integration
         * @param nameMapping Mapping from source node names to target node names
         * @param createdNodes Collection of newly created nodes
         */
        void updateNodeConnections(Model & model,
                                  Model & target,
                                  nodes::FunctionCall const & functionCall,
                                  std::unordered_map<std::string, std::string> const & nameMapping,
                                  std::vector<NodeBase *> const & createdNodes);

        /**
         * @brief Connects an input parameter to a corresponding function call input
         * @param target Target model
         * @param functionCall Function call that contains the input
         * @param parameter Parameter to connect
         * @param sourcePortName Source port name to look for in function call inputs
         */
        void connectBeginNodeInput(Model & target,
                                  nodes::FunctionCall const & functionCall,
                                  VariantParameter & parameter,
                                  std::string const & sourcePortName);

        /**
         * @brief Connects an input parameter to a corresponding port in a regular node
         * @param target Target model
         * @param parameter Parameter to connect
         * @param originalSourceNodeName Original source node name
         * @param sourcePortName Source port name
         * @param nameMapping Mapping from source node names to target node names
         */
        void connectRegularNodeInput(Model & target,
                                    VariantParameter & parameter,
                                    std::string const & originalSourceNodeName,
                                    std::string const & sourcePortName,
                                    std::unordered_map<std::string, std::string> const & nameMapping);

        void rerouteOutputs(Model & model, Model & target, nodes::FunctionCall const & functionCall, std::unordered_map<std::string, std::string> const & nameMapping);

        size_t m_flatteningDepth = 0;
        
        /**
         * @brief Counts nodes that would be added from function calls in a model
         * @param model The model to examine function calls in
         * @param countedModels Set of models that have already been counted
         * @param totalNodeCount Reference to running total of node count
         */
        void countNodesFromFunctionCalls(
            Model const & model, 
            std::unordered_set<ResourceId> & countedModels, 
            size_t & totalNodeCount);
    };
}