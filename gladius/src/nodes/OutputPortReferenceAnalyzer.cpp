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

        // First pass: Mark reachable nodes
        markReachableNodes();

        // Second pass: Count references for reachable nodes by visiting all nodes
        m_model->visitNodes(*this);

        m_analyzed = true;
    }

    void OutputPortReferenceAnalyzer::visit(NodeBase & node)
    {
        if (isNodeReachable(node.getId()))
        {
            analyzeNode(node);
        }
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

    void OutputPortReferenceAnalyzer::markReachableNodes()
    {
        m_reachableNodes.clear();

        if (!m_model)
        {
            return;
        }

        auto endNode = m_model->getEndNode();
        if (!endNode)
        {
            return;
        }

        // Start backward traversal from End node
        traverseBackward(endNode->getId());
    }

    void OutputPortReferenceAnalyzer::traverseBackward(NodeId nodeId)
    {
        // Check if already visited
        if (m_reachableNodes.contains(nodeId))
        {
            return;
        }

        // Mark as reachable
        m_reachableNodes.insert(nodeId);

        // Get the node
        auto nodeOpt = m_model->getNode(nodeId);
        if (!nodeOpt.has_value())
        {
            return;
        }

        auto node = nodeOpt.value();

        // Traverse all input parameters
        for (auto & [paramName, param] : node->parameter())
        {
            auto const & source = param.getConstSource();
            if (source.has_value())
            {
                // Recursively traverse to the producer node
                traverseBackward(source->nodeId);
            }
        }
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
