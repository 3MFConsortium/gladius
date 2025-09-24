#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace gladius::nodes
{
    class Model;
    class NodeBase;
    using NodeId = int;
}

namespace gladius
{
    /**
     * @brief Converts Gladius node graphs back to mathematical expressions
     *
     * This class takes a node graph and converts it back into a mathematical
     * expression string. It's the reverse operation of ExpressionToGraphConverter
     * and is useful for editing existing graphs or verification.
     */
    class GraphToExpressionConverter
    {
      public:
        /// Constructor
        GraphToExpressionConverter();

        /// Destructor
        ~GraphToExpressionConverter();

        /**
         * @brief Convert a node graph to a mathematical expression
         * @param model The model containing the node graph
         * @param outputNodeId The output node to start conversion from
         * @return The mathematical expression string, empty if conversion failed
         */
        std::string convertGraphToExpression(nodes::Model const & model,
                                             nodes::NodeId outputNodeId);

        /**
         * @brief Get the last error message from conversion
         * @return Error message string, empty if no error
         */
        std::string getLastError() const;

        /**
         * @brief Check if the last conversion was successful
         * @return true if the last conversion succeeded
         */
        bool hasSucceeded() const;

      private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace gladius
