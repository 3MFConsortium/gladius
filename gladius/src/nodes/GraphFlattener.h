#pragma once

#include "Assembly.h"
#include <unordered_set>

namespace gladius::nodes
{
    class GraphFlattener
    {
      public:
        GraphFlattener(Assembly const & assembly);

        /**
         * @brief Flatten the graph, so the assembly will only have one single function (model)
         */
        Assembly flatten();

      private:
        Assembly m_assembly;
        std::unordered_set<ResourceId> m_usedFunctions;

        void findUsedFunctions();
        void findUsedFunctionsInModel(Model & model);
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