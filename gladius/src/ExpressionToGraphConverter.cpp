#include "ExpressionToGraphConverter.h"
#include "ExpressionParser.h"
#include "nodes/Model.h"
#include "nodes/DerivedNodes.h"
#include "nodes/NodeBase.h"
#include "nodes/nodesfwd.h"

#include <algorithm>
#include <cctype>
#include <stack>
#include <stdexcept>

namespace gladius
{
    nodes::NodeId ExpressionToGraphConverter::convertExpressionToGraph(
        std::string const& expression, 
        nodes::Model& model,
        ExpressionParser& parser)
    {
        // First validate the expression
        if (!parser.parseExpression(expression) || !parser.hasValidExpression())
        {
            return 0; // Invalid expression
        }

        // Get variables from the expression
        std::vector<std::string> variables = parser.getVariables();
        
        // Create input nodes for variables
        std::map<std::string, nodes::NodeId> variableNodes = createVariableNodes(variables, model);

        // Parse the expression and build the graph
        return parseAndBuildGraph(expression, model, variableNodes);
    }

    std::map<std::string, nodes::NodeId> ExpressionToGraphConverter::createVariableNodes(
        std::vector<std::string> const& variables,
        nodes::Model& model)
    {
        std::map<std::string, nodes::NodeId> variableNodes;

        for (std::string const& varName : variables)
        {
            // Create a parameter input node for each variable
            // We'll use ConstantScalar nodes that can be used as inputs
            nodes::NodeBase* node = nodes::createNodeFromName("ConstantScalar", model);
            if (node)
            {
                variableNodes[varName] = node->getId();
                
                // Set the display name to the variable name
                node->setDisplayName(varName);
                
                // TODO: Set the default value if needed
                // This would require access to the node's parameters
            }
        }

        return variableNodes;
    }

    nodes::NodeId ExpressionToGraphConverter::createMathOperationNode(
        std::string const& operation,
        nodes::Model& model)
    {
        nodes::NodeBase* node = nodes::createNodeFromName(operation, model);
        return node ? node->getId() : 0;
    }

    nodes::NodeId ExpressionToGraphConverter::createConstantNode(
        double value,
        nodes::Model& model)
    {
        nodes::NodeBase* node = nodes::createNodeFromName("ConstantScalar", model);
        if (node)
        {
            // TODO: Set the constant value
            // This would require setting the node's parameter value
            node->setDisplayName(std::to_string(value));
            return node->getId();
        }
        return 0;
    }

    bool ExpressionToGraphConverter::connectNodes(
        nodes::Model& model,
        nodes::NodeId fromNodeId,
        std::string const& fromPortName,
        nodes::NodeId toNodeId,
        std::string const& toPortName)
    {
        // Find the source node and get its output port
        auto fromNodeOpt = model.getNode(fromNodeId);
        if (!fromNodeOpt.has_value())
        {
            return false;
        }
        nodes::NodeBase* fromNode = fromNodeOpt.value();

        nodes::Port* outputPort = fromNode->findOutputPort(fromPortName);
        if (!outputPort)
        {
            return false;
        }

        // Find the target node and get its input parameter
        auto toNodeOpt = model.getNode(toNodeId);
        if (!toNodeOpt.has_value())
        {
            return false;
        }
        nodes::NodeBase* toNode = toNodeOpt.value();

        nodes::VariantParameter* inputParam = toNode->getParameter(toPortName);
        if (!inputParam)
        {
            return false;
        }

        // Create the link using the model's addLink method
        return model.addLink(outputPort->getId(), inputParam->getId());
    }

    nodes::NodeId ExpressionToGraphConverter::parseAndBuildGraph(
        std::string const& expression,
        nodes::Model& model,
        std::map<std::string, nodes::NodeId> const& variableNodes)
    {
        std::string cleanExpr = removeWhitespace(expression);
        
        if (cleanExpr.empty())
        {
            return 0;
        }

        // Handle parentheses - find outermost expression
        if (cleanExpr.front() == '(' && cleanExpr.back() == ')')
        {
            // Check if these parentheses wrap the entire expression
            int depth = 0;
            bool wrapsEntire = true;
            for (size_t i = 0; i < cleanExpr.length() - 1; ++i)
            {
                if (cleanExpr[i] == '(') depth++;
                else if (cleanExpr[i] == ')') depth--;
                
                if (depth == 0)
                {
                    wrapsEntire = false;
                    break;
                }
            }
            
            if (wrapsEntire)
            {
                return parseAndBuildGraph(cleanExpr.substr(1, cleanExpr.length() - 2), model, variableNodes);
            }
        }

        // Check if it's a variable
        auto varIt = variableNodes.find(cleanExpr);
        if (varIt != variableNodes.end())
        {
            return varIt->second;
        }

        // Check if it's a number
        try
        {
            double value = std::stod(cleanExpr);
            return createConstantNode(value, model);
        }
        catch (...)
        {
            // Not a number, continue parsing
        }

        // Find the main operator
        auto [operatorPos, operatorChar] = findMainOperator(cleanExpr);
        
        if (operatorPos == std::string::npos)
        {
            // No operator found, could be a function call
            // For now, return 0 (unsupported)
            return 0;
        }

        // Split the expression at the operator
        std::string leftExpr = cleanExpr.substr(0, operatorPos);
        std::string rightExpr = cleanExpr.substr(operatorPos + 1);

        // Recursively build left and right nodes
        nodes::NodeId leftNodeId = parseAndBuildGraph(leftExpr, model, variableNodes);
        nodes::NodeId rightNodeId = parseAndBuildGraph(rightExpr, model, variableNodes);

        if (leftNodeId == 0 || rightNodeId == 0)
        {
            return 0; // Failed to build sub-expressions
        }

        // Create the operation node
        std::string operationName;
        switch (operatorChar)
        {
            case '+': operationName = "Addition"; break;
            case '-': operationName = "Subtraction"; break;
            case '*': operationName = "Multiplication"; break;
            case '/': operationName = "Division"; break;
            default: return 0; // Unsupported operator
        }

        nodes::NodeId operationNodeId = createMathOperationNode(operationName, model);
        if (operationNodeId == 0)
        {
            return 0;
        }

        // Connect the input nodes to the operation node
        // Determine the correct output port names for the source nodes
        std::string leftPortName = getOutputPortName(model, leftNodeId);
        std::string rightPortName = getOutputPortName(model, rightNodeId);
        
        bool leftConnected = connectNodes(model, leftNodeId, leftPortName, operationNodeId, nodes::FieldNames::A);
        bool rightConnected = connectNodes(model, rightNodeId, rightPortName, operationNodeId, nodes::FieldNames::B);

        if (!leftConnected || !rightConnected)
        {
            // If connection failed, we should still return the operation node
            // as it was created successfully, even if not properly connected
        }

        return operationNodeId;
    }

    std::string ExpressionToGraphConverter::removeWhitespace(std::string const& expr)
    {
        std::string result;
        result.reserve(expr.length());
        
        for (char c : expr)
        {
            if (!std::isspace(c))
            {
                result += c;
            }
        }
        
        return result;
    }

    std::pair<size_t, char> ExpressionToGraphConverter::findMainOperator(std::string const& expr)
    {
        int depth = 0;
        int minPrecedence = INT_MAX;
        size_t operatorPos = std::string::npos;
        char operatorChar = 0;

        // Scan from right to left to handle left-associative operators correctly
        for (int i = static_cast<int>(expr.length()) - 1; i >= 0; --i)
        {
            char c = expr[i];
            
            if (c == ')') depth++;
            else if (c == '(') depth--;
            else if (depth == 0 && isOperator(c))
            {
                int precedence = getOperatorPrecedence(c);
                if (precedence <= minPrecedence)
                {
                    minPrecedence = precedence;
                    operatorPos = i;
                    operatorChar = c;
                }
            }
        }

        return {operatorPos, operatorChar};
    }

    std::string ExpressionToGraphConverter::getOutputPortName(nodes::Model& model, nodes::NodeId nodeId)
    {
        auto nodeOpt = model.getNode(nodeId);
        if (!nodeOpt.has_value())
        {
            return nodes::FieldNames::Value; // Default fallback
        }
        nodes::NodeBase* node = nodeOpt.value();

        // Check if the node has a "Result" output port (math operations)
        if (node->findOutputPort(nodes::FieldNames::Result))
        {
            return nodes::FieldNames::Result;
        }
        
        // Otherwise, assume it has a "Value" output port (constants, variables)
        return nodes::FieldNames::Value;
    }

    bool ExpressionToGraphConverter::isOperator(char c)
    {
        return c == '+' || c == '-' || c == '*' || c == '/';
    }

    int ExpressionToGraphConverter::getOperatorPrecedence(char op)
    {
        switch (op)
        {
            case '+':
            case '-':
                return 1;
            case '*':
            case '/':
                return 2;
            default:
                return 0;
        }
    }

} // namespace gladius
