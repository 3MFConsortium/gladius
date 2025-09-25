#include "GraphToExpressionConverter.h"
#include "nodes/DerivedNodes.h"
#include "nodes/Model.h"
#include "nodes/NodeBase.h"
#include "nodes/nodesfwd.h"

#include <sstream>
#include <unordered_set>

namespace gladius
{
    class GraphToExpressionConverter::Impl
    {
      public:
        std::string m_lastError;
        bool m_hasSucceeded = false;

        std::string convertGraphToExpression(nodes::Model const & model, nodes::NodeId outputNodeId)
        {
            m_lastError.clear();
            m_hasSucceeded = false;

            // Get the output node
            auto nodeOpt = model.getNode(outputNodeId);
            if (!nodeOpt.has_value())
            {
                m_lastError = "Output node not found";
                return "";
            }

            nodes::NodeBase * outputNode = nodeOpt.value();
            std::unordered_set<nodes::NodeId> visitedNodes;

            std::string result = convertNodeToExpression(model, outputNode, visitedNodes);

            if (!result.empty())
            {
                m_hasSucceeded = true;
            }
            else if (m_lastError.empty())
            {
                m_lastError = "Failed to convert graph to expression";
            }

            return result;
        }

      private:
        std::string convertNodeToExpression(nodes::Model const & model,
                                            nodes::NodeBase * node,
                                            std::unordered_set<nodes::NodeId> & visitedNodes)
        {
            if (!node)
            {
                m_lastError = "Null node encountered";
                return "";
            }

            // Check for circular dependencies
            if (visitedNodes.find(node->getId()) != visitedNodes.end())
            {
                m_lastError = "Circular dependency detected";
                return "";
            }

            visitedNodes.insert(node->getId());

            std::string nodeName = node->name();

            // Handle different node types
            if (nodeName == "Addition")
            {
                return convertBinaryOperationNode(model, node, "+", visitedNodes);
            }
            else if (nodeName == "Subtraction")
            {
                return convertBinaryOperationNode(model, node, "-", visitedNodes);
            }
            else if (nodeName == "Multiplication")
            {
                return convertBinaryOperationNode(model, node, "*", visitedNodes);
            }
            else if (nodeName == "Division")
            {
                return convertBinaryOperationNode(model, node, "/", visitedNodes);
            }
            else if (nodeName == "ConstantScalar")
            {
                return convertConstantNode(node);
            }
            else
            {
                // Assume it's a variable or input node
                return convertVariableNode(node);
            }
        }

        std::string convertBinaryOperationNode(nodes::Model const & model,
                                               nodes::NodeBase * node,
                                               std::string const & operatorSymbol,
                                               std::unordered_set<nodes::NodeId> & visitedNodes)
        {
            // Get the A and B input parameters
            nodes::VariantParameter * paramA = node->getParameter(nodes::FieldNames::A);
            nodes::VariantParameter * paramB = node->getParameter(nodes::FieldNames::B);

            if (!paramA || !paramB)
            {
                m_lastError = "Binary operation node missing A or B parameter";
                return "";
            }

            // Get the source nodes for A and B
            std::string leftExpr = getExpressionFromParameter(model, paramA, visitedNodes);
            std::string rightExpr = getExpressionFromParameter(model, paramB, visitedNodes);

            if (leftExpr.empty() || rightExpr.empty())
            {
                return "";
            }

            // Handle operator precedence - add parentheses if needed
            bool needsParentheses = false;
            if (operatorSymbol == "*" || operatorSymbol == "/")
            {
                // For multiplication and division, add parentheses around addition/subtraction
                needsParentheses = (leftExpr.find('+') != std::string::npos ||
                                    leftExpr.find('-') != std::string::npos ||
                                    rightExpr.find('+') != std::string::npos ||
                                    rightExpr.find('-') != std::string::npos);
            }

            std::ostringstream result;
            if (needsParentheses && (leftExpr.find('+') != std::string::npos ||
                                     leftExpr.find('-') != std::string::npos))
            {
                result << "(" << leftExpr << ")";
            }
            else
            {
                result << leftExpr;
            }

            result << " " << operatorSymbol << " ";

            if (needsParentheses && (rightExpr.find('+') != std::string::npos ||
                                     rightExpr.find('-') != std::string::npos))
            {
                result << "(" << rightExpr << ")";
            }
            else
            {
                result << rightExpr;
            }

            return result.str();
        }

        std::string convertConstantNode(nodes::NodeBase * node)
        {
            // For constant nodes, we could try to get the value from the parameter
            // For now, we'll use the display name which should contain the value
            std::string displayName = node->getDisplayName();

            // If display name is empty or default, return a placeholder
            if (displayName.empty() || displayName == node->name())
            {
                return "1.0"; // Default constant value
            }

            return displayName;
        }

        std::string convertVariableNode(nodes::NodeBase * node)
        {
            // For variable nodes, use the display name
            std::string displayName = node->getDisplayName();

            // If display name is empty or default, use a generic variable name
            if (displayName.empty() || displayName == node->name())
            {
                return "x"; // Default variable name
            }

            return displayName;
        }

        std::string getExpressionFromParameter(nodes::Model const & model,
                                               nodes::VariantParameter * param,
                                               std::unordered_set<nodes::NodeId> & visitedNodes)
        {
            if (!param->getSource().has_value())
            {
                // Parameter has no source, might be a direct value
                // For now, return a placeholder
                return "0";
            }

            // Get the source port
            auto source = param->getSource().value();

            // Find the node that owns this port
            // This is a simplified approach - in practice, we'd need to traverse the port registry
            // For now, we'll return a placeholder indicating we need to implement port-to-node
            // mapping

            // TODO: Implement proper port-to-node mapping
            // This would require accessing the model's internal structure to find which node owns a
            // given port

            return "variable"; // Placeholder
        }
    };

    GraphToExpressionConverter::GraphToExpressionConverter()
        : m_impl(std::make_unique<Impl>())
    {
    }

    GraphToExpressionConverter::~GraphToExpressionConverter() = default;

    std::string GraphToExpressionConverter::convertGraphToExpression(nodes::Model const & model,
                                                                     nodes::NodeId outputNodeId)
    {
        return m_impl->convertGraphToExpression(model, outputNodeId);
    }

    std::string GraphToExpressionConverter::getLastError() const
    {
        return m_impl->m_lastError;
    }

    bool GraphToExpressionConverter::hasSucceeded() const
    {
        return m_impl->m_hasSucceeded;
    }

} // namespace gladius
