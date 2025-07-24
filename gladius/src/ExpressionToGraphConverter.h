#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>

namespace gladius
{
    class ExpressionParser;
}

namespace gladius::nodes
{
    class Model;
    class NodeBase;
    using NodeId = int;
}

namespace gladius
{
    /**
     * @brief Converts mathematical expressions to Gladius node graphs
     * 
     * This class takes a parsed mathematical expression and creates the corresponding
     * node graph in a Gladius model. It handles the creation of math operation nodes,
     * variable input nodes, and the connections between them.
     */
    class ExpressionToGraphConverter
    {
    public:
        /**
         * @brief Convert an expression to a node graph
         * @param expression The mathematical expression as a string
         * @param model The model to add nodes to
         * @param parser The expression parser to use
         * @return The NodeId of the output node, or 0 if conversion failed
         */
        static nodes::NodeId convertExpressionToGraph(
            std::string const& expression, 
            nodes::Model& model,
            ExpressionParser& parser
        );

    private:
        /**
         * @brief Create variable input nodes for the expression
         * @param variables List of variable names found in the expression
         * @param model The model to add nodes to
         * @return Map of variable names to their corresponding node IDs
         */
        static std::map<std::string, nodes::NodeId> createVariableNodes(
            std::vector<std::string> const& variables,
            nodes::Model& model
        );

        /**
         * @brief Create a mathematical operation node
         * @param operation The operation name (e.g., "Addition", "Multiplication")
         * @param model The model to add the node to
         * @return The NodeId of the created node, or 0 if creation failed
         */
        static nodes::NodeId createMathOperationNode(
            std::string const& operation,
            nodes::Model& model
        );

        /**
         * @brief Create a constant value node
         * @param value The constant value
         * @param model The model to add the node to
         * @return The NodeId of the created node, or 0 if creation failed
         */
        static nodes::NodeId createConstantNode(
            double value,
            nodes::Model& model
        );

        /**
         * @brief Connect two nodes via their ports
         * @param model The model containing the nodes
         * @param fromNodeId Source node ID
         * @param fromPortName Source port name
         * @param toNodeId Destination node ID
         * @param toPortName Destination port name
         * @return true if connection was successful
         */
        static bool connectNodes(
            nodes::Model& model,
            nodes::NodeId fromNodeId,
            std::string const& fromPortName,
            nodes::NodeId toNodeId,
            std::string const& toPortName
        );

        /**
         * @brief Parse expression manually and build graph recursively
         * 
         * Since muParser doesn't provide AST access, we'll implement a simple
         * recursive descent parser for basic mathematical expressions.
         * 
         * @param expression The expression to parse
         * @param model The model to add nodes to
         * @param variableNodes Map of variable names to node IDs
         * @return The NodeId of the result node, or 0 if parsing failed
         */
        static nodes::NodeId parseAndBuildGraph(
            std::string const& expression,
            nodes::Model& model,
            std::map<std::string, nodes::NodeId> const& variableNodes
        );

        /**
         * @brief Remove whitespace from expression
         */
        static std::string removeWhitespace(std::string const& expr);

        /**
         * @brief Find the main operator in an expression (respecting precedence)
         */
        static std::pair<size_t, char> findMainOperator(std::string const& expr);

        /**
         * @brief Check if a character is a valid operator
         */
        static bool isOperator(char c);

        /**
         * @brief Get operator precedence
         */
        static int getOperatorPrecedence(char op);

        /**
         * @brief Get the correct output port name for a node
         */
        static std::string getOutputPortName(nodes::Model& model, nodes::NodeId nodeId);
    };

} // namespace gladius
