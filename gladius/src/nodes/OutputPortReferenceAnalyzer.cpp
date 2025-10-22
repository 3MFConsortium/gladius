#include "OutputPortReferenceAnalyzer.h"
#include "Model.h"
#include "NodeBase.h"
#include "Parameter.h"
#include <algorithm>

namespace gladius::nodes
{
    void OutputPortReferenceAnalyzer::setModel(Model * model)
    {
        m_model = model;
        m_analyzed = false;
        clear();
    }

    void OutputPortReferenceAnalyzer::analyze()
    {
        if (!m_model)
        {
            return;
        }

        clear();

        // Ensure graph and topological order are up to date
        m_model->updateGraphAndOrderIfNeeded();

        // First pass: Identify reachable nodes using reverse topological order
        markReachableNodes();

        // Second pass: Count references only for reachable nodes
        auto const & outputOrder = m_model->getOutputOrder();

        for (NodeId const nodeId : outputOrder)
        {
            // Skip unreachable nodes
            if (!m_reachableNodes.contains(nodeId))
            {
                continue;
            }

            auto nodeOpt = m_model->getNode(nodeId);
            if (!nodeOpt.has_value())
            {
                continue;
            }

            auto const & node = nodeOpt.value();
            analyzeNode(*node);
        }

        m_analyzed = true;
    }

    void OutputPortReferenceAnalyzer::markReachableNodes()
    {
        m_reachableNodes.clear();

        if (!m_model)
        {
            return;
        }

        // Ensure graph is up to date before analyzing
        m_model->updateGraphAndOrderIfNeeded();

        auto endNode = m_model->getEndNode();
        if (!endNode)
        {
            return;
        }

        // Mark End node as reachable
        m_reachableNodes.insert(endNode->getId());

        // Iterate through nodes in REVERSE topological order
        // This ensures that consumers are processed before producers
        auto const & outputOrder = m_model->getOutputOrder();

        for (auto it = outputOrder.rbegin(); it != outputOrder.rend(); ++it)
        {
            NodeId const nodeId = *it;

            // Skip if already marked as reachable
            if (m_reachableNodes.contains(nodeId))
            {
                continue;
            }

            auto nodeOpt = m_model->getNode(nodeId);
            if (!nodeOpt.has_value())
            {
                continue;
            }

            auto const & node = nodeOpt.value();

            // Check if this node feeds into any reachable node
            bool feedsReachableNode = false;

            // For each output port of this node
            for (auto const & [portName, port] : node->getOutputs())
            {
                // Check all nodes in the model to find consumers of this port
                for (NodeId const potentialConsumerId : outputOrder)
                {
                    // Only interested in nodes that are already marked as reachable
                    if (!m_reachableNodes.contains(potentialConsumerId))
                    {
                        continue;
                    }

                    auto consumerOpt = m_model->getNode(potentialConsumerId);
                    if (!consumerOpt.has_value())
                    {
                        continue;
                    }

                    auto const & consumerNode = consumerOpt.value();

                    // Check each parameter of the consumer node
                    for (auto const & [paramName, param] : consumerNode->parameter())
                    {
                        auto const & source = param.getConstSource();
                        
                        // If this parameter is connected to our node's output
                        if (source.has_value() && 
                            source->nodeId == nodeId &&
                            source->shortName == portName)
                        {
                            // This node feeds a reachable node!
                            feedsReachableNode = true;
                            break;
                        }
                    }

                    if (feedsReachableNode)
                    {
                        break; // No need to check more consumers
                    }
                }

                if (feedsReachableNode)
                {
                    break; // No need to check more outputs
                }
            }

            // If this node feeds any reachable node, mark it as reachable
            if (feedsReachableNode)
            {
                m_reachableNodes.insert(nodeId);
            }
        }
    }

    void OutputPortReferenceAnalyzer::visit(NodeBase & node)
    {
        // Note: This method is kept for Visitor interface compatibility
        // but analyze() now directly iterates through nodes in topological order
        analyzeNode(node);
    }

    void OutputPortReferenceAnalyzer::analyzeNode(NodeBase & node)
    {
        NodeId const consumingNodeId = node.getId();

        // Analyze all input parameters of this node
        for (auto & [paramName, param] : node.parameter())
        {
            recordReference(param, consumingNodeId);
        }
    }

    void OutputPortReferenceAnalyzer::recordReference(VariantParameter const & param,
                                                       NodeId consumingNodeId)
    {
        auto const & source = param.getConstSource();
        if (!source.has_value())
        {
            // Parameter is not connected to an output port
            return;
        }

        PortReference const producerRef{source->nodeId, source->shortName};
        PortReference const consumerRef{consumingNodeId, ""};

        // Increment reference count
        m_referenceCounts[producerRef]++;

        // Record consumer
        m_consumers[producerRef].push_back(consumerRef);
    }

    size_t OutputPortReferenceAnalyzer::getReferenceCount(NodeId nodeId,
                                                           PortName const & portName) const
    {
        PortReference const ref{nodeId, portName};
        auto it = m_referenceCounts.find(ref);
        if (it != m_referenceCounts.end())
        {
            return it->second;
        }
        return 0;
    }

    bool OutputPortReferenceAnalyzer::shouldInline(NodeId nodeId, PortName const & portName) const
    {
        // Only inline if referenced exactly once
        return getReferenceCount(nodeId, portName) == 1;
    }

    bool OutputPortReferenceAnalyzer::isNodeReachable(NodeId nodeId) const
    {
        return m_reachableNodes.contains(nodeId);
    }

    std::vector<PortReference>
    OutputPortReferenceAnalyzer::getConsumers(NodeId nodeId, PortName const & portName) const
    {
        PortReference const ref{nodeId, portName};
        auto it = m_consumers.find(ref);
        if (it != m_consumers.end())
        {
            return it->second;
        }
        return {};
    }

    void OutputPortReferenceAnalyzer::clear()
    {
        m_referenceCounts.clear();
        m_consumers.clear();
        m_reachableNodes.clear();
        m_analyzed = false;
    }

} // namespace gladius::nodes
