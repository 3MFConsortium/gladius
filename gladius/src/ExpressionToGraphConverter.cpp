#include "ExpressionToGraphConverter.h"
#include "ExpressionParser.h"
#include "FunctionArgument.h"
#include "nodes/DerivedNodes.h"
#include "nodes/Model.h"
#include "nodes/NodeBase.h"
#include "nodes/nodesfwd.h"
#include "nodes/types.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <regex>
#include <set>
#include <stack>
#include <stdexcept>

namespace gladius
{
    // Static member definitions
    std::map<nodes::NodeId, std::string> ExpressionToGraphConverter::s_componentMap;
    std::map<std::string, nodes::NodeId> ExpressionToGraphConverter::s_vectorDecomposeNodes;
    std::map<std::string, ArgumentType> ExpressionToGraphConverter::s_beginNodeArguments;

    // Track the current variable context for Begin node port resolution
    thread_local std::string ExpressionToGraphConverter::s_currentVariableContext;

    nodes::NodeId ExpressionToGraphConverter::convertExpressionToGraph(
      std::string const & expression,
      nodes::Model & model,
      ExpressionParser & parser,
      std::vector<FunctionArgument> const & arguments)
    {
        // Clear any previous component mappings
        s_componentMap.clear();
        s_vectorDecomposeNodes.clear();
        s_beginNodeArguments.clear();

        // Debug: Check expression parsing
        bool parseResult = parser.parseExpression(expression);
        bool hasValid = parser.hasValidExpression();

        if (!parseResult)
        {
            return 0; // Invalid expression
        }

        if (!hasValid)
        {
            return 0; // Invalid expression
        }

        // Get variables from the expression (these are in original form like "pos.x")
        std::vector<std::string> variables = parser.getVariables();

        // Create input nodes based on arguments or variables
        std::map<std::string, nodes::NodeId> variableNodes;

        if (!arguments.empty())
        {
            // Use function arguments to create properly typed input nodes
            variableNodes = createArgumentNodes(arguments, model);

            // Debug: Check if argument nodes were created
            if (variableNodes.empty())
            {
                return 0;
            }

            // Note: Component access will be handled later in parseAndBuildGraph
            // when it encounters expressions like "pos.x"
        }
        else
        {
            // Legacy mode: create variable nodes without type information
            variableNodes = createVariableNodes(variables, model, arguments);
        }

        // Parse the expression and build the graph
        nodes::NodeId result = parseAndBuildGraph(expression, model, variableNodes);

        // Debug: Check if parseAndBuildGraph succeeded
        if (result == 0)
        {
            // parseAndBuildGraph failed
        }

        return result;
    }

    std::map<std::string, nodes::NodeId>
    ExpressionToGraphConverter::createVariableNodes(std::vector<std::string> const & variables,
                                                    nodes::Model & model,
                                                    std::vector<FunctionArgument> const & arguments)
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

        // Check if it's a component access (e.g., "A.x", "pos.y")
        if (isComponentAccess(cleanExpr))
        {
            return parseComponentAccess(cleanExpr, variableNodes, model);
        }

        // Check if it's a preprocessed component access (e.g., "pos_x" -> "pos.x")
        if (isPreprocessedComponentAccess(cleanExpr))
        {
            std::string originalForm = convertPreprocessedToOriginal(cleanExpr);
            return parseComponentAccess(originalForm, variableNodes, model);
        }

        // Check if it's a variable
        auto varIt = variableNodes.find(cleanExpr);
        if (varIt != variableNodes.end())
        {
            // Set the current variable context for Begin node port resolution
            s_currentVariableContext = cleanExpr;
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

        // Check if this is a Begin node - if so, use the current variable context
        if (dynamic_cast<nodes::Begin *>(node) != nullptr)
        {
            if (!s_currentVariableContext.empty())
            {
                // Clear the context after use
                std::string context = s_currentVariableContext;
                s_currentVariableContext.clear();
                return context; // Return the argument name as the port name
            }
            return nodes::FieldNames::Value; // Fallback
        }

        // Check if this is a DecomposeVector node with component information
        auto componentIt = s_componentMap.find(nodeId);
        if (componentIt != s_componentMap.end())
        {
            std::string const & component = componentIt->second;
            if (component == "x")
                return nodes::FieldNames::X;
            else if (component == "y")
                return nodes::FieldNames::Y;
            else if (component == "z")
                return nodes::FieldNames::Z;
        }

        // Check if the node has a "Result" output port (math operations)
        if (node->findOutputPort(nodes::FieldNames::Result))
        {
            return nodes::FieldNames::Result;
        }

        // Check if the node has a "Vector" output port (ConstantVector nodes)
        if (node->findOutputPort(nodes::FieldNames::Vector))
        {
            return nodes::FieldNames::Vector;
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

    std::map<std::string, nodes::NodeId>
    ExpressionToGraphConverter::createArgumentNodes(std::vector<FunctionArgument> const & arguments,
                                                    nodes::Model & model)
    {
        std::map<std::string, nodes::NodeId> argumentNodes;

        // Ensure Begin/End nodes exist for function arguments
        if (!model.getBeginNode())
        {
            model.createBeginEnd();
        }

        nodes::Begin * beginNode = model.getBeginNode();

        for (auto const & arg : arguments)
        {
            if (arg.type == ArgumentType::Scalar)
            {
                // Add scalar argument to Begin node
                nodes::VariantParameter parameter(float{0.0f});
                model.addArgument(arg.name, parameter);
            }
            else if (arg.type == ArgumentType::Vector)
            {
                // Add vector argument to Begin node
                nodes::VariantParameter parameter(nodes::float3{0.0f, 0.0f, 0.0f});
                model.addArgument(arg.name, parameter);
            }

            // Store the Begin node ID as the source for this argument
            // and track the argument type for port resolution
            argumentNodes[arg.name] = beginNode->getId();
            s_beginNodeArguments[arg.name] = arg.type; // Store argument type
        }

        // Register outputs and update node IDs after adding all arguments
        if (!arguments.empty())
        {
            model.registerOutputs(*beginNode);
            beginNode->updateNodeIds();
        }

        return argumentNodes;
    }

    nodes::NodeId ExpressionToGraphConverter::parseComponentAccess(
      std::string const & expression,
      std::map<std::string, nodes::NodeId> const & argumentNodes,
      nodes::Model & model)
    {
        auto [argName, component] = parseComponentExpression(expression);

        if (argName.empty() || component.empty())
        {
            return 0; // Invalid component access
        }

        // Find the argument node
        auto it = argumentNodes.find(argName);
        if (it == argumentNodes.end())
        {
            return 0; // Argument not found
        }

        // Validate that the argument node is a vector type (not scalar)
        auto nodeOpt = model.getNode(it->second);
        if (!nodeOpt.has_value())
        {
            return 0; // Node not found
        }

        nodes::NodeBase * argNode = nodeOpt.value();
        std::string nodeTypeName = argNode->name();

        // Allow component access on vector nodes (ConstantVector) or Begin nodes with vector
        // arguments
        bool isVector = (nodeTypeName.find("ConstantVector") != std::string::npos);
        bool isBeginNode =
          (nodeTypeName == "Input" || dynamic_cast<nodes::Begin *>(argNode) != nullptr);

        if (!isVector && !isBeginNode)
        {
            return 0; // Component access not allowed on non-vector types
        }

        // For Begin nodes, we need to verify the argument is actually a vector type
        if (isBeginNode)
        {
            // Check if this argument was registered as a vector argument
            auto argIt = s_beginNodeArguments.find(argName);
            if (argIt == s_beginNodeArguments.end())
            {
                return 0; // Argument not found in Begin node arguments
            }
            if (argIt->second != ArgumentType::Vector)
            {
                return 0; // Argument is not a vector type, component access not allowed
            }
        }

        // Check if we already have a DecomposeVector node for this vector
        auto decomposeIt = s_vectorDecomposeNodes.find(argName);
        nodes::NodeBase * decomposeNode = nullptr;

        if (decomposeIt != s_vectorDecomposeNodes.end())
        {
            // Reuse existing DecomposeVector node
            auto nodeOpt = model.getNode(decomposeIt->second);
            if (nodeOpt.has_value())
            {
                decomposeNode = nodeOpt.value();
            }
        }

        if (!decomposeNode)
        {
            // Create a new DecomposeVector node
            decomposeNode = nodes::createNodeFromName("DecomposeVector", model);
            if (!decomposeNode)
            {
                return 0; // Failed to create decompose node
            }

            // Connect the vector argument to the decompose node
            // For Begin nodes, set the context so getOutputPortName knows which argument to use
            if (isBeginNode)
            {
                s_currentVariableContext = argName;
            }

            if (!connectNodes(model,
                              it->second,
                              getOutputPortName(model, it->second),
                              decomposeNode->getId(),
                              nodes::FieldNames::A))
            {
                return 0; // Failed to connect
            }

            // Store the DecomposeVector node for reuse
            s_vectorDecomposeNodes[argName] = decomposeNode->getId();
        }

        // Create a unique identifier for this component access
        std::string componentAccessKey = expression; // Use the full expression like "pos.x"

        // Map this DecomposeVector node to the specific component
        // This will be used later when connecting nodes
        s_componentMap[decomposeNode->getId()] = component;

        // Return the DecomposeVector node directly
        return decomposeNode->getId();
    }

    bool ExpressionToGraphConverter::isComponentAccess(std::string const & expression)
    {
        // Check if it contains a dot, but is not at start or end
        size_t dotPos = expression.find('.');
        if (dotPos == std::string::npos || dotPos == 0 || dotPos == expression.length() - 1)
        {
            return false;
        }

        // Check if the expression is ONLY a component access (variable.component)
        // This means no operators should be present
        std::string operatorChars = "+-*/()";
        for (char op : operatorChars)
        {
            if (expression.find(op) != std::string::npos)
            {
                return false; // Contains operators, so it's not a simple component access
            }
        }

        // Check if there's exactly one dot
        if (expression.find('.', dotPos + 1) != std::string::npos)
        {
            return false; // Multiple dots
        }

        // Check if the part before the dot is a valid identifier
        std::string varName = expression.substr(0, dotPos);
        if (varName.empty() || !std::isalpha(varName[0]))
        {
            return false;
        }

        // Check if the part after the dot is a valid component (x, y, or z)
        std::string component = expression.substr(dotPos + 1);
        return component == "x" || component == "y" || component == "z";
    }

    std::pair<std::string, std::string>
    ExpressionToGraphConverter::parseComponentExpression(std::string const & expression)
    {
        size_t dotPos = expression.find('.');
        if (dotPos == std::string::npos || dotPos == 0 || dotPos == expression.length() - 1)
        {
            return {"", ""}; // Invalid format
        }

        std::string argName = expression.substr(0, dotPos);
        std::string component = expression.substr(dotPos + 1);

        // Validate component name
        if (component != "x" && component != "y" && component != "z")
        {
            return {"", ""}; // Invalid component
        }

        return {argName, component};
    }

    bool ExpressionToGraphConverter::isPreprocessedComponentAccess(std::string const & expression)
    {
        // Check if it matches pattern: identifier_[xyz]
        std::regex preprocessedRegex(R"([a-zA-Z][a-zA-Z0-9_]*_[xyz])");
        return std::regex_match(expression, preprocessedRegex);
    }

    std::string
    ExpressionToGraphConverter::convertPreprocessedToOriginal(std::string const & expression)
    {
        // Convert pos_x -> pos.x
        size_t underscorePos = expression.rfind('_');
        if (underscorePos != std::string::npos && underscorePos > 0 &&
            underscorePos < expression.length() - 1)
        {
            char component = expression[underscorePos + 1];
            if (component == 'x' || component == 'y' || component == 'z')
            {
                std::string argName = expression.substr(0, underscorePos);
                return argName + "." + component;
            }
        }
        return expression; // Return unchanged if not valid preprocessed form
    }

    nodes::NodeBase *
    ExpressionToGraphConverter::createComponentExtractorNode(std::string const & component,
                                                             nodes::Model & model)
    {
        // Create a simple pass-through node using multiplication by 1.0
        // This effectively creates a node that takes one input and outputs the same value
        nodes::NodeBase * multiplyNode = nodes::createNodeFromName("Multiplication", model);
        if (!multiplyNode)
        {
            return nullptr;
        }

        // Create a constant node with value 1.0 for the second input
        nodes::NodeId constantNodeId = createConstantNode(1.0, model);
        if (constantNodeId == 0)
        {
            return nullptr;
        }

        // Connect the constant 1.0 to the second input of the multiplication node
        if (!connectNodes(model,
                          constantNodeId,
                          nodes::FieldNames::Value,
                          multiplyNode->getId(),
                          nodes::FieldNames::B))
        {
            return nullptr;
        }

        return multiplyNode;
    }

} // namespace gladius
