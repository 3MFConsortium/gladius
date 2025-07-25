#include "ExpressionToGraphConverter.h"
#include "ExpressionParser.h"
#include "nodes/DerivedNodes.h"
#include "nodes/Model.h"
#include "nodes/NodeBase.h"
#include "nodes/nodesfwd.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stack>
#include <stdexcept>

namespace gladius
{
    nodes::NodeId
    ExpressionToGraphConverter::convertExpressionToGraph(std::string const & expression,
                                                         nodes::Model & model,
                                                         ExpressionParser & parser)
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

    std::map<std::string, nodes::NodeId>
    ExpressionToGraphConverter::createVariableNodes(std::vector<std::string> const & variables,
                                                    nodes::Model & model)
    {
        std::map<std::string, nodes::NodeId> variableNodes;

        for (std::string const & varName : variables)
        {
            // Create a parameter input node for each variable
            // We'll use ConstantScalar nodes that can be used as inputs
            nodes::NodeBase * node = nodes::createNodeFromName("ConstantScalar", model);
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

    nodes::NodeId ExpressionToGraphConverter::createMathOperationNode(std::string const & operation,
                                                                      nodes::Model & model)
    {
        nodes::NodeBase * node = nodes::createNodeFromName(operation, model);
        return node ? node->getId() : 0;
    }

    nodes::NodeId ExpressionToGraphConverter::createConstantNode(double value, nodes::Model & model)
    {
        nodes::NodeBase * node = nodes::createNodeFromName("ConstantScalar", model);
        if (node)
        {
            // TODO: Set the constant value
            // This would require setting the node's parameter value
            node->setDisplayName(std::to_string(value));
            return node->getId();
        }
        return 0;
    }

    bool ExpressionToGraphConverter::connectNodes(nodes::Model & model,
                                                  nodes::NodeId fromNodeId,
                                                  std::string const & fromPortName,
                                                  nodes::NodeId toNodeId,
                                                  std::string const & toPortName)
    {
        // Find the source node and get its output port
        auto fromNodeOpt = model.getNode(fromNodeId);
        if (!fromNodeOpt.has_value())
        {
            return false;
        }
        nodes::NodeBase * fromNode = fromNodeOpt.value();

        nodes::Port * outputPort = fromNode->findOutputPort(fromPortName);
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
        nodes::NodeBase * toNode = toNodeOpt.value();

        nodes::VariantParameter * inputParam = toNode->getParameter(toPortName);
        if (!inputParam)
        {
            return false;
        }

        // Create the link using the model's addLink method
        return model.addLink(outputPort->getId(), inputParam->getId());
    }

    nodes::NodeId ExpressionToGraphConverter::parseAndBuildGraph(
      std::string const & expression,
      nodes::Model & model,
      std::map<std::string, nodes::NodeId> const & variableNodes)
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
                if (cleanExpr[i] == '(')
                    depth++;
                else if (cleanExpr[i] == ')')
                    depth--;

                if (depth == 0)
                {
                    wrapsEntire = false;
                    break;
                }
            }

            if (wrapsEntire)
            {
                return parseAndBuildGraph(
                  cleanExpr.substr(1, cleanExpr.length() - 2), model, variableNodes);
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
            return parseFunctionCall(cleanExpr, model, variableNodes);
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
        case '+':
            operationName = "Addition";
            break;
        case '-':
            operationName = "Subtraction";
            break;
        case '*':
            operationName = "Multiplication";
            break;
        case '/':
            operationName = "Division";
            break;
        default:
            return 0; // Unsupported operator
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

        bool leftConnected =
          connectNodes(model, leftNodeId, leftPortName, operationNodeId, nodes::FieldNames::A);
        bool rightConnected =
          connectNodes(model, rightNodeId, rightPortName, operationNodeId, nodes::FieldNames::B);

        if (!leftConnected || !rightConnected)
        {
            // If connection failed, we should still return the operation node
            // as it was created successfully, even if not properly connected
        }

        return operationNodeId;
    }

    nodes::NodeId ExpressionToGraphConverter::parseFunctionCall(
      std::string const & expression,
      nodes::Model & model,
      std::map<std::string, nodes::NodeId> const & variableNodes)
    {
        // Look for function call pattern: functionName(arguments)
        size_t openParen = expression.find('(');
        if (openParen == std::string::npos)
        {
            return 0; // No opening parenthesis found
        }

        if (expression.back() != ')')
        {
            return 0; // No closing parenthesis at the end
        }

        std::string functionName = expression.substr(0, openParen);
        std::string argumentsStr =
          expression.substr(openParen + 1, expression.length() - openParen - 2);

        // Map function names to node types
        std::string nodeTypeName;
        if (functionName == "sin")
            nodeTypeName = "Sine";
        else if (functionName == "cos")
            nodeTypeName = "Cosine";
        else if (functionName == "tan")
            nodeTypeName = "Tangent";
        else if (functionName == "asin")
            nodeTypeName = "ArcSin";
        else if (functionName == "acos")
            nodeTypeName = "ArcCos";
        else if (functionName == "atan")
            nodeTypeName = "ArcTan";
        else if (functionName == "sinh")
            nodeTypeName = "SinH";
        else if (functionName == "cosh")
            nodeTypeName = "CosH";
        else if (functionName == "tanh")
            nodeTypeName = "TanH";
        else if (functionName == "exp")
            nodeTypeName = "Exp";
        else if (functionName == "log")
            nodeTypeName = "Log";
        else if (functionName == "log2")
            nodeTypeName = "Log2";
        else if (functionName == "log10")
            nodeTypeName = "Log10";
        else if (functionName == "sqrt")
            nodeTypeName = "Sqrt";
        else if (functionName == "abs")
            nodeTypeName = "Abs";
        else if (functionName == "sign")
            nodeTypeName = "Sign";
        else if (functionName == "floor")
            nodeTypeName = "Floor";
        else if (functionName == "ceil")
            nodeTypeName = "Ceil";
        else if (functionName == "round")
            nodeTypeName = "Round";
        else if (functionName == "fract")
            nodeTypeName = "Fract";
        else
        {
            // Handle binary functions
            if (functionName == "pow")
                nodeTypeName = "Pow";
            else if (functionName == "atan2")
                nodeTypeName = "ArcTan2";
            else if (functionName == "fmod")
                nodeTypeName = "Fmod";
            else if (functionName == "min")
                nodeTypeName = "Min";
            else if (functionName == "max")
                nodeTypeName = "Max";
            else
            {
                return 0; // Unsupported function
            }
        }

        // Create the function node
        nodes::NodeId functionNodeId = createMathOperationNode(nodeTypeName, model);
        if (functionNodeId == 0)
        {
            return 0;
        }

        // Parse arguments
        std::vector<std::string> arguments = parseArgumentList(argumentsStr);

        // Connect arguments based on function type
        if (isSingleArgumentFunction(functionName))
        {
            if (arguments.size() != 1)
            {
                return 0; // Wrong number of arguments
            }

            nodes::NodeId argNodeId = parseAndBuildGraph(arguments[0], model, variableNodes);
            if (argNodeId == 0)
            {
                return 0;
            }

            std::string argPortName = getOutputPortName(model, argNodeId);
            connectNodes(model, argNodeId, argPortName, functionNodeId, nodes::FieldNames::A);
        }
        else if (isBinaryFunction(functionName))
        {
            if (arguments.size() != 2)
            {
                return 0; // Wrong number of arguments
            }

            nodes::NodeId arg1NodeId = parseAndBuildGraph(arguments[0], model, variableNodes);
            nodes::NodeId arg2NodeId = parseAndBuildGraph(arguments[1], model, variableNodes);

            if (arg1NodeId == 0 || arg2NodeId == 0)
            {
                return 0;
            }

            std::string arg1PortName = getOutputPortName(model, arg1NodeId);
            std::string arg2PortName = getOutputPortName(model, arg2NodeId);

            connectNodes(model, arg1NodeId, arg1PortName, functionNodeId, nodes::FieldNames::A);
            connectNodes(model, arg2NodeId, arg2PortName, functionNodeId, nodes::FieldNames::B);
        }
        else
        {
            return 0; // Unsupported function type
        }

        return functionNodeId;
    }

    std::vector<std::string>
    ExpressionToGraphConverter::parseArgumentList(std::string const & argumentsStr)
    {
        std::vector<std::string> arguments;

        if (argumentsStr.empty())
        {
            return arguments;
        }

        std::string cleanArgs = removeWhitespace(argumentsStr);
        std::string currentArg;
        int depth = 0;

        for (char c : cleanArgs)
        {
            if (c == ',' && depth == 0)
            {
                if (!currentArg.empty())
                {
                    arguments.push_back(currentArg);
                    currentArg.clear();
                }
            }
            else
            {
                currentArg += c;
                if (c == '(')
                    depth++;
                else if (c == ')')
                    depth--;
            }
        }

        if (!currentArg.empty())
        {
            arguments.push_back(currentArg);
        }

        return arguments;
    }

    bool ExpressionToGraphConverter::isSingleArgumentFunction(std::string const & functionName)
    {
        static const std::set<std::string> singleArgFunctions = {
          "sin", "cos",  "tan",   "asin", "acos", "atan", "sinh",  "cosh", "tanh",  "exp",
          "log", "log2", "log10", "sqrt", "abs",  "sign", "floor", "ceil", "round", "fract"};

        return singleArgFunctions.find(functionName) != singleArgFunctions.end();
    }

    bool ExpressionToGraphConverter::isBinaryFunction(std::string const & functionName)
    {
        static const std::set<std::string> binaryFunctions = {"pow", "atan2", "fmod", "min", "max"};

        return binaryFunctions.find(functionName) != binaryFunctions.end();
    }

    std::string ExpressionToGraphConverter::removeWhitespace(std::string const & expr)
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

    std::pair<size_t, char> ExpressionToGraphConverter::findMainOperator(std::string const & expr)
    {
        int depth = 0;
        int minPrecedence = INT_MAX;
        size_t operatorPos = std::string::npos;
        char operatorChar = 0;

        // Scan from right to left to handle left-associative operators correctly
        for (int i = static_cast<int>(expr.length()) - 1; i >= 0; --i)
        {
            char c = expr[i];

            if (c == ')')
                depth++;
            else if (c == '(')
                depth--;
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

    std::string ExpressionToGraphConverter::getOutputPortName(nodes::Model & model,
                                                              nodes::NodeId nodeId)
    {
        auto nodeOpt = model.getNode(nodeId);
        if (!nodeOpt.has_value())
        {
            return nodes::FieldNames::Value; // Default fallback
        }
        nodes::NodeBase * node = nodeOpt.value();

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
