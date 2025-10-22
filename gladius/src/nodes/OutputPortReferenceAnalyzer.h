#pragma once

#include "Visitor.h"
#include "nodesfwd.h"
#include "Parameter.h"
#include <map>
#include <set>
#include <vector>

namespace gladius::nodes
{
    /// @brief Identifies output port referenced by a parameter
    struct PortReference
    {
        NodeId nodeId{};
        PortName portName{};

        bool operator<(PortReference const & other) const
        {
            if (nodeId != other.nodeId)
            {
                return nodeId < other.nodeId;
            }
            return portName < other.portName;
        }

        bool operator==(PortReference const & other) const
        {
            return nodeId == other.nodeId && portName == other.portName;
        }
    };

    /// @brief Analyzes the node graph to determine how many times each output port is referenced
    /// This information is used to optimize code generation by inlining single-use expressions
    class OutputPortReferenceAnalyzer : public Visitor
    {
      public:
        OutputPortReferenceAnalyzer() = default;
        ~OutputPortReferenceAnalyzer() override = default;

        /// @brief Set the model to analyze
        void setModel(Model * model) override;

        /// @brief Analyze the model to build reference counts
        void analyze();

        /// @brief Get the number of times an output port is referenced
        /// @param nodeId The node ID
        /// @param portName The output port name
        /// @return The number of references
        [[nodiscard]] size_t getReferenceCount(NodeId nodeId, PortName const & portName) const;

        /// @brief Check if an output should be inlined (referenced exactly once)
        /// @param nodeId The node ID
        /// @param portName The output port name
        /// @return true if the output should be inlined
        [[nodiscard]] bool shouldInline(NodeId nodeId, PortName const & portName) const;

        /// @brief Check if a node is reachable from the End node
        /// @param nodeId The node ID
        /// @return true if the node is reachable
        [[nodiscard]] bool isNodeReachable(NodeId nodeId) const;

        /// @brief Get all consumers of an output port
        /// @param nodeId The node ID
        /// @param portName The output port name
        /// @return Vector of port references that consume this output
        [[nodiscard]] std::vector<PortReference> getConsumers(NodeId nodeId,
                                                               PortName const & portName) const;

        /// @brief Clear all analysis data
        void clear();

        // Visitor interface
        void visit(NodeBase & node) override;

      private:
        /// @brief Analyze a single node's parameters
        void analyzeNode(NodeBase & node);

        /// @brief Record a reference from parameter to output port
        void recordReference(VariantParameter const & param, NodeId consumingNodeId);

        /// @brief Mark reachable nodes using reverse topological order iteration
        /// This efficiently identifies which nodes can influence the End node
        void markReachableNodes();

        Model * m_model = nullptr;
        std::map<PortReference, size_t> m_referenceCounts;
        std::map<PortReference, std::vector<PortReference>> m_consumers;
        std::set<NodeId> m_reachableNodes;
        bool m_analyzed = false;
    };

} // namespace gladius::nodes
