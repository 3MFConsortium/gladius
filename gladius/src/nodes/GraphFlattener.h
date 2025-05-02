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

        void rerouteOutputs(Model & model, Model & target, nodes::FunctionCall const & functionCall, std::unordered_map<std::string, std::string> const & nameMapping);

        size_t m_flatteningDepth = 0;
    };
}