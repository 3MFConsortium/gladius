#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "FunctionArgument.h"

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
         * @param arguments Optional function arguments (for vector support)
         * @return The NodeId of the output node, or 0 if conversion failed
         */
        static nodes::NodeId
        convertExpressionToGraph(std::string const & expression,
                                 nodes::Model & model,
                                 ExpressionParser & parser,
                                 std::vector<FunctionArgument> const & arguments = {});

      private:
        /**
         * @brief Create variable input nodes for the expression
         * @param variables List of variable names found in the expression
         * @param model The model to add nodes to
         * @param arguments Function arguments for type information
         * @return Map of variable names to their corresponding node IDs
         */
        static std::map<std::string, nodes::NodeId>
        createVariableNodes(std::vector<std::string> const & variables,
                            nodes::Model & model,
                            std::vector<FunctionArgument> const & arguments);

        /**
         * @brief Create argument input nodes based on function arguments
         * @param arguments Function arguments definition
         * @param model The model to add nodes to
         * @return Map of argument names to their corresponding node IDs
         */
        static std::map<std::string, nodes::NodeId>
        createArgumentNodes(std::vector<FunctionArgument> const & arguments, nodes::Model & model);

        /**
         * @brief Parse component access expression (e.g., "A.x", "pos.y")
         * @param expression The component access expression
         * @param argumentNodes Map of argument names to node IDs
         * @param model The model to add nodes to
         * @return The NodeId of the component output, or 0 if parsing failed
         */
        static nodes::NodeId
        parseComponentAccess(std::string const & expression,
                             std::map<std::string, nodes::NodeId> const & argumentNodes,
                             nodes::Model & model);

        /**
         * @brief Create a component extractor node (pass-through for vector components)
         * @param component The component name ("x", "y", or "z")
         * @param model The model to add the node to
         * @return The created node, or nullptr if creation failed
         */
        static nodes::NodeBase * createComponentExtractorNode(std::string const & component,
                                                              nodes::Model & model);

        /**
         * @brief Track DecomposeVector node component mappings
         */
        static std::map<nodes::NodeId, std::string> s_componentMap;

        /**
         * @brief Track vector nodes to reuse DecomposeVector nodes
         */
        static std::map<std::string, nodes::NodeId> s_vectorDecomposeNodes;

        /**
         * @brief Track Begin node argument output ports and their types
         * Maps argument name to ArgumentType for Begin node outputs
         */
        static std::map<std::string, ArgumentType> s_beginNodeArguments;

        /**
         * @brief Track current variable context for Begin node port resolution
         */
        static thread_local std::string s_currentVariableContext;

        /**
         * @brief Create a mathematical operation node
         * @param operation The operation name (e.g., "Addition", "Multiplication")
         * @param model The model to add the node to
         * @return The NodeId of the created node, or 0 if creation failed
         */
        static nodes::NodeId createMathOperationNode(std::string const & operation,
                                                     nodes::Model & model);

        /**
         * @brief Create a constant value node
         * @param value The constant value
         * @param model The model to add the node to
         * @return The NodeId of the created node, or 0 if creation failed
         */
        static nodes::NodeId createConstantNode(double value, nodes::Model & model);

        /**
         * @brief Connect two nodes via their ports
         * @param model The model containing the nodes
         * @param fromNodeId Source node ID
         * @param fromPortName Source port name
         * @param toNodeId Destination node ID
         * @param toPortName Destination port name
         * @return true if connection was successful
         */
        static bool connectNodes(nodes::Model & model,
                                 nodes::NodeId fromNodeId,
                                 std::string const & fromPortName,
                                 nodes::NodeId toNodeId,
                                 std::string const & toPortName);

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
        static nodes::NodeId
        parseAndBuildGraph(std::string const & expression,
                           nodes::Model & model,
                           std::map<std::string, nodes::NodeId> const & variableNodes);

        /**
         * @brief Parse a function call and create the appropriate math node
         *
         * @param expression The function call expression (e.g., "sin(x)", "pow(x,2)")
         * @param model The model to add nodes to
         * @param variableNodes Map of variable names to node IDs
         * @return The NodeId of the result node, or 0 if parsing failed
         */
        static nodes::NodeId
        parseFunctionCall(std::string const & expression,
                          nodes::Model & model,
                          std::map<std::string, nodes::NodeId> const & variableNodes);

        /**
         * @brief Parse a comma-separated argument list
         * @param argumentsStr The arguments string (without parentheses)
         * @return Vector of argument expressions
         */
        static std::vector<std::string> parseArgumentList(std::string const & argumentsStr);

        /**
         * @brief Check if a function takes only one argument
         */
        static bool isSingleArgumentFunction(std::string const & functionName);

        /**
         * @brief Check if a function takes two arguments
         */
        static bool isBinaryFunction(std::string const & functionName);

        /**
         * @brief Remove whitespace from expression
         */
        static std::string removeWhitespace(std::string const & expr);

        /**
         * @brief Find the main operator in an expression (respecting precedence)
         */
        static std::pair<size_t, char> findMainOperator(std::string const & expr);

        /**
         * @brief Check if a character is a valid operator
         */
        static bool isOperator(char c);

        /**
         * @brief Get operator precedence
         */
        static int getOperatorPrecedence(char op);

        /**
         * @brief Check if an expression is a component access (e.g., "A.x")
         * @param expression The expression to check
         * @return true if it's a component access expression
         */
        static bool isComponentAccess(std::string const & expression);

        /**
         * @brief Extract argument name and component from component access expression
         * @param expression The component access expression (e.g., "A.x")
         * @return Pair of argument name and component name, or empty strings if invalid
         */
        static std::pair<std::string, std::string>
        parseComponentExpression(std::string const & expression);

        /**
         * @brief Get the correct output port name for a node
         */
        static std::string getOutputPortName(nodes::Model & model, nodes::NodeId nodeId);

        /**
         * @brief Check if an expression is a preprocessed component access (e.g., "pos_x")
         * @param expression The expression to check
         * @return true if it's a preprocessed component access expression
         */
        static bool isPreprocessedComponentAccess(std::string const & expression);

        /**
         * @brief Convert preprocessed component access back to original form
         * @param expression The preprocessed expression (e.g., "pos_x")
         * @return Original form (e.g., "pos.x"), or unchanged if not valid
         */
        static std::string convertPreprocessedToOriginal(std::string const & expression);
    };

} // namespace gladius
