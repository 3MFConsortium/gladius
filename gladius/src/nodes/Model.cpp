#include "Model.h"
#include "Assembly.h"
#include "EventLogger.h"
#include "exceptions.h"
#include "graph/GraphAlgorithms.h"
#include "nodesfwd.h"

#include <algorithm>
#include <iostream>
#include <string>

namespace gladius::nodes
{

    void printNodeTypes()
    {
        NodeTypes nodeTypes;
        staticFor(nodeTypes,
                  [&](auto i, auto & node)
                  {
                      (void) i;
                      std::cout << node.name() << "\n";
                  });
    }

    bool equalsCaseInsensitive(std::string const & a, std::string const & b)
    {
        return std::equal(a.begin(),
                          a.end(),
                          b.begin(),
                          b.end(),
                          [](char a, char b) { return tolower(a) == tolower(b); });
    }

    NodeBase * createNodeFromName(const std::string & name, Model & nodes)
    {
        NodeBase * createdNode{nullptr};
        NodeTypes nodeTypes;

        staticFor(nodeTypes,
                  [&](auto, auto & node)
                  {
                      if (equalsCaseInsensitive(node.name(), name))
                      {
                          createdNode = nodes.create(node);
                          return;
                      }
                  });
        return createdNode;
    }

    Model::Model(const Model & other)
        : m_lastParameterId(other.m_lastParameterId)
        , m_lastId(other.m_lastId)
        , m_graph(other.m_graph)
        , m_outputOrder(other.m_outputOrder)
        , m_graphRequiresUpdate(other.m_graphRequiresUpdate)
        , m_name(other.m_name)
        , m_displayName(other.m_displayName)
        , m_resourceId(other.m_resourceId)
        , m_isManaged(other.m_isManaged)
        , m_allInputReferencesAreValid(other.m_allInputReferencesAreValid)
        , m_nodesHaveBeenLayouted(other.m_nodesHaveBeenLayouted)
        , m_isValid(other.m_isValid)
    {
        m_outPorts.clear();
        m_inputParameter.clear();
        m_nodes.clear();

        auto cloneNode = [&](auto & node)
        {
            m_nodes[node.second->getId()] = node.second->clone();
            auto & clonedNode = m_nodes[node.second->getId()];
            clonedNode->updateNodeIds();
        };

        for (auto & node : other.m_nodes)
        {
            cloneNode(node);
        }

        for (auto & node : m_nodes)
        {
            for (auto & port : node.second->getOutputs())
            {
                m_outPorts[port.second.getId()] = (&(port.second));
            }

            registerInputs(*node.second);
            node.second->setUniqueName(node.second->getUniqueName()); //???
            node.second->setId(node.second->getId());
        }

        auto beginVisitor =
          OnTypeVisitor<Begin>([&](auto & beginNode) { m_beginNode = &beginNode; });
        visitNodes(beginVisitor);

        auto endVisitor = OnTypeVisitor<End>([&](auto & endNode) { m_endNode = &endNode; });
        visitNodes(endVisitor);

        for (const auto & node : other.m_nodes)
        {
            for (auto & parameter : node.second->parameter())
            {
                if (parameter.second.getSource().has_value())
                {
                    const auto portIter =
                      std::find_if(std::begin(m_outPorts),
                                   std::end(m_outPorts),
                                   [&](auto & port)
                                   {
                                       return port.second->getUniqueName() ==
                                              parameter.second.getSource().value().uniqueName; //
                                   });

                    if (portIter == std::end(m_outPorts))
                    {
                        throw std::runtime_error(
                          fmt::format("Output port with the name {} could not be found",
                                      parameter.second.getSource().value().uniqueName));
                    }
                    bool const skipLinkValidation = true;
                    addLink(
                      portIter->second->getId(), parameter.second.getId(), skipLinkValidation);
                }

                parameter.second.setConsumedByFunction(parameter.second.isConsumedByFunction());
            }
        }

        this->updateTypes();
        this->updateOrder();
    }

    std::string extractArgumentName(const std::string & partParameterName,
                                    std::string extendedArgumentName)
    {
        const std::string strToRemove = partParameterName + "_";
        const auto pos = extendedArgumentName.find(strToRemove);

        if (pos != std::string::npos)
        {
            extendedArgumentName.erase(pos, strToRemove.length());
        }
        return extendedArgumentName;
    }

    void Model::updatePartArguments(NodeId partNodeId,
                                    Model & referencedModel,
                                    const std::string & partParameterName)
    {
        const auto partNodeOpt = getNode(partNodeId);
        if (!partNodeOpt.has_value() || partNodeOpt.value() == nullptr)
        {
            return;
        }

        auto * partNode = partNodeOpt.value();
        auto * beginNode = referencedModel.getBeginNode();

        if (beginNode == nullptr)
        {
            throw std::runtime_error("beginNode of referenced Model is not set");
        }

        // Erase existing arguments, that are not contained in the new argument list
        for (auto iter = std::begin(partNode->parameter());
             iter != std::end(partNode->parameter());)
        {
            const auto originalArgumentName = extractArgumentName(partParameterName, iter->first);
            const bool isStillAnArgument = beginNode->getOutputs().find(originalArgumentName) !=
                                           std::end(beginNode->getOutputs());
            if (iter->second.isArgument() && !isStillAnArgument &&
                (iter->second.getArgumentAssoziation().empty() ||
                 (iter->second.getArgumentAssoziation() == partParameterName)))
            {
                iter = partNode->parameter().erase(iter);
            }
            else
            {
                ++iter;
            }
        }

        for (auto & [name, parameter] : beginNode->parameter())
        {
            if (name == FieldNames::Pos)
            {
                continue;
            }

            const auto extendedName = partParameterName + std::string{"_"} + name;
            if (partNode->parameter().find(extendedName) == std::end(partNode->parameter()))
            {
                float initialValue = 0.f;
                auto & initialParameter = parameter.Value();
                if (const auto initialValuePtr = std::get_if<float>(&initialParameter))
                {
                    initialValue = *initialValuePtr;
                }

                partNode->parameter()[extendedName] =
                  VariantParameter(initialValue, parameter.getContentType());
                partNode->parameter()[extendedName].setArgumentAssoziation(partParameterName);
                registerInput(partNode->parameter()[extendedName]);
            }

            if (partNode->getOutputs().find(extendedName) == std::end(partNode->getOutputs()))
            {
                partNode->addOutputPort(extendedName, ParameterTypeIndex::Float);
                partNode->updateNodeIds();
                registerOutput(partNode->getOutputs()[extendedName]);
                partNode->getOutputs()[extendedName].hide();
            }
        }
    }

    void Model::setModelName(const ModelName & modelName)
    {
        m_name = modelName;
    }

    void Model::registerInput(VariantParameter & parameter)
    {
        if (m_inputParameter.find(parameter.getId()) != m_inputParameter.end())
        {
            return;
        }
        if (parameter.getId() < 1)
        {
            parameter.setId(20000 + ++m_lastParameterId);
        }
        m_inputParameter[parameter.getId()] = &parameter;
    }

    void Model::registerOutput(Port & port)
    {
        if (port.getId() < 1)
        {
            const auto uppderBound =
              std::max_element(std::begin(m_outPorts),
                               std::end(m_outPorts),
                               [](auto & lhs, auto & rhs) { return lhs.first < rhs.first; });
            if (uppderBound != std::end(m_outPorts))
            {
                port.setId(std::max(10000, uppderBound->first + 1));
            }
            else
            {
                port.setId(10000 + static_cast<int>(m_outPorts.size()));
            }
        }
        m_outPorts[port.getId()] = (&port);
    }

    bool Model::isNodeNameOccupied(const NodeName & name)
    {
        return findNode(name) != nullptr;
    }

    NodeBase * Model::findNode(const NodeName & name)
    {
        const auto iter =
          std::find_if(m_nodes.begin(),
                       m_nodes.end(),
                       [name](auto & node) { return node.second->getUniqueName() == name; });
        if (iter == m_nodes.end())
        {
            return nullptr;
        }

        return iter->second.get();
    }

    std::string & Model::getModelName()
    {
        return m_name;
    }

    Begin * Model::getBeginNode()
    {
        return m_beginNode;
    }

    End * Model::getEndNode()
    {
        return m_endNode;
    }

    void Model::addArgument(ParameterName name, VariantParameter parameter)
    {
        if (m_beginNode == nullptr)
        {
            return;
        }
        m_beginNode->parameter()[name] = parameter;
        m_beginNode->addOutputPort(name, parameter.getTypeIndex());
        registerInput(m_beginNode->parameter()[name]);
        registerOutput(m_beginNode->getOutputs()[name]);
    }

    void Model::addFunctionOutput(ParameterName name, VariantParameter parameter)
    {
        if (m_endNode == nullptr)
        {
            return;
        }

        m_endNode->parameter()[name] = parameter;
        registerInput(m_endNode->parameter()[name]);
    }

    PortRegistry & Model::getPortRegistry()
    {
        return m_outPorts;
    }

    const graph::AdjacencyListDirectedGraph & Model::getGraph() const
    {
        return m_graph;
    }

    InputParameterRegistry & Model::getParameterRegistry()
    {
        return m_inputParameter;
    }

    InputParameterRegistry const & Model::getConstParameterRegistry() const
    {
        return m_inputParameter;
    }

    std::optional<NodeBase *> Model::getNode(NodeId id) const
    {
        const auto node = m_nodes.find(id);
        if (node != std::end(m_nodes))
        {
            return {node->second.get()};
        }
        return {};
    }

    void Model::updateGraphAndOrderIfNeeded()
    {
        try
        {
            if (m_graphRequiresUpdate)
            {
                buildGraph();
                m_outputOrder = topologicalSort(m_graph);
                m_graphRequiresUpdate = false;
                updateOrder();
            }
        }
        catch (const std::exception & e)
        {
            if (m_logger)
            {
                m_logger->addEvent({e.what(), events::Severity::Error});
            }
            std::cerr << e.what() << "\n";
        }
    }

    void Model::updateOrder()
    {
        NodeId order{};
        updateGraphAndOrderIfNeeded();

        for (const auto id : m_outputOrder)
        {
            auto node = m_nodes.find(id);
            if (node != std::end(m_nodes))
            {
                node->second->setOrder(++order);
            }
        }
    }

    void Model::visitNodes(Visitor & visitor)
    {
        updateGraphAndOrderIfNeeded();
        visitor.setModel(this);
        for (const auto id : m_outputOrder)
        {
            auto node = m_nodes.find(id);
            if (node != std::end(m_nodes))
            {
                try
                {
                    node->second->accept(visitor);
                }
                catch (const std::exception & e)
                {
                    std::cerr << e.what() << '\n';
                    throw e;
                }
            }
        }
    }

    bool Model::removeLink(const PortId startId, const ParameterId endId)
    {
        const auto sourcePort = m_outPorts.find(startId);
        if (sourcePort == std::end(m_outPorts))
        {
            return false;
        }
        const auto targetParameter = m_inputParameter.find(endId);
        if (targetParameter == std::end(m_inputParameter))
        {
            return false;
        }

        targetParameter->second->getSource().reset();
        m_graphRequiresUpdate = true;
        return true;
    }

    bool Model::isLinkValid(Port & source, VariantParameter & target)
    {
        if (source.getParentId() == target.getParentId())
        {
            if (m_logger)
            {
                m_logger->addEvent({"Cannot link parameter to itself", events::Severity::Warning});
            }
            return false;
        }

        updateGraphAndOrderIfNeeded();
        auto const isSourceSuccessor =
          isDependingOn(getGraph(), source.getParentId(), target.getParentId());

        return !isSourceSuccessor;
    }

    bool Model::isLinkValid(PortId const sourceId, ParameterId const targetId)
    {
        const auto sourcePort = m_outPorts.find(sourceId);
        if (sourcePort == std::end(m_outPorts))
        {
            return false;
        }
        const auto targetParameter = m_inputParameter.find(targetId);
        if (targetParameter == std::end(m_inputParameter))
        {
            return false;
        }

        auto const variantTarget = dynamic_cast<VariantParameter *>(targetParameter->second);

        if ((variantTarget != nullptr) && (!isLinkValid(*sourcePort->second, *variantTarget)))
        {
            return false;
        }

        return true;
    }

    bool Model::addLink(const PortId startId, const ParameterId endId, bool skipCheck)
    {
        const auto sourcePort = m_outPorts.find(startId);

        if (sourcePort == std::end(m_outPorts))
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  {fmt::format("Soure port {} not found", startId), events::Severity::Error});
            }
            return false;
        }
        const auto targetParameter = m_inputParameter.find(endId);
        if (targetParameter == std::end(m_inputParameter))
        {
            if (m_logger)
            {
                m_logger->addEvent({fmt::format("Target parameter with id {} not found", endId),
                                    events::Severity::Error});
            }
            return false;
        }

        auto const variantTarget = dynamic_cast<VariantParameter *>(targetParameter->second);

        if ((variantTarget == nullptr))
        {
            return false;
        }

        if (!skipCheck)
        {
            if (!isLinkValid(*sourcePort->second, *variantTarget))
            {
                return false;
            }
        }

        targetParameter->second->setInputFromPort(*sourcePort->second);
        invalidateGraph();

        if (!skipCheck)
        {
            return updateTypes();
        }

        return true;
    }

    void Model::remove(NodeId id)
    {
        const auto nodeToRemove = m_nodes.find(id);

        // First check if the node exists
        if (nodeToRemove == std::end(m_nodes))
        {
            return;
        }

        // Protect begin node from deletion
        if (m_beginNode && nodeToRemove->second->getId() == m_beginNode->getId())
        {
            return;
        }

        // Protect end node from deletion
        if (m_endNode && nodeToRemove->second->getId() == m_endNode->getId())
        {
            return;
        }

        if (nodeToRemove != std::end(m_nodes))
        {
            updateGraphAndOrderIfNeeded();
            const auto successor = determineSuccessor(m_graph, id);
            for (auto consumerId : successor)
            {
                auto consumerNode = m_nodes.find(consumerId);

                if (consumerNode != std::end(m_nodes))
                {

                    for (auto & in : consumerNode->second->parameter())
                    {
                        if (in.second.getSource())
                        {
                            const auto srcPort = m_outPorts[in.second.getSource().value().portId];
                            if ((srcPort != nullptr) && srcPort->getParentId() == id)
                            {
                                in.second.getSource().reset();
                            }
                        }
                    }
                }
            }

            for (auto & [name, param] : nodeToRemove->second->parameter())
            {
                auto paramIter = m_inputParameter.find(param.getId());
                if (param.getParentId() != nodeToRemove->second->getId())
                {
                    // Log warning instead of throwing - this can happen in edge cases
                    if (m_logger)
                    {
                        m_logger->addEvent(
                          {fmt::format("Parameter {} has incorrect parent ID {} instead of {}",
                                       name,
                                       param.getParentId(),
                                       nodeToRemove->second->getId()),
                           events::Severity::Warning});
                    }
                    continue; // Skip this parameter instead of throwing
                }
                if (paramIter != std::end(m_inputParameter))
                {
                    m_inputParameter.erase(paramIter);
                }
            }

            m_nodes.erase(nodeToRemove);
        }
        m_graphRequiresUpdate = true;
        updateGraphAndOrderIfNeeded();
    }

    void Model::removeNodeWithoutLinks(NodeId idOfNodeWithoutLinks)
    {
        const auto nodeToRemove = m_nodes.find(idOfNodeWithoutLinks);
        if (nodeToRemove != std::end(m_nodes))
        {
            m_nodes.erase(nodeToRemove);
        }
        m_graphRequiresUpdate = true;
    }

    void Model::createBeginEnd()
    {
        m_beginNode = create<Begin>();
        m_beginNode->setDisplayName("inputs");
        m_beginNode->screenPos() =
          nodes::float2(0.0f, 0.0f); ///< Set initial screen position for Begin node
        m_endNode = create<End>();
        m_endNode->setDisplayName("outputs");
        m_endNode->screenPos() =
          nodes::float2(400.0f, 0.0f); ///< Set initial screen position for End node
    }

    void Model::createBeginEndWithDefaultInAndOuts()
    {
        createBeginEnd();
        m_beginNode->addOutputPort(FieldNames::Pos, ParameterTypeIndex::Float3);
        registerOutputs(*m_beginNode);
        m_endNode->parameter()[FieldNames::Shape] = VariantParameter(float{-1.f});
        m_endNode->parameter()[FieldNames::Color] = VariantParameter(float3{0.5f, 0.5f, 0.5f});

        registerInputs(*m_endNode);
        m_beginNode->updateNodeIds();
        m_endNode->updateNodeIds();
    }

    void Model::createValidVoid()
    {
        createBeginEndWithDefaultInAndOuts();
        auto constNode = create<ConstantScalar>();
        constNode->parameter()[FieldNames::Value] = VariantParameter(float{FLT_MAX});
        addLink(constNode->getOutputs().begin()->second.getId(),
                m_endNode->parameter().at(FieldNames::Shape).getId());
    }

    graph::AdjacencyListDirectedGraph & Model::buildGraph()
    {
        m_allInputReferencesAreValid = false;
        m_graph = graph::AdjacencyListDirectedGraph(m_lastId);

        // Add all nodes as vertices to ensure they are included in topological sort
        // even if they have no connections
        for (auto const & [id, node] : m_nodes)
        {
            m_graph.addVertex(id);
        }

        for (auto & [id, node] : m_nodes)
        {
            for (auto & parameter : node->parameter())
            {
                if (parameter.second.getSource().has_value())
                {
                    auto srcIter = m_outPorts.find(parameter.second.getSource().value().portId);
                    if (srcIter != std::end(m_outPorts))
                    {
                        const auto srcId = srcIter->second->getParentId();
                        m_graph.addDependency(id, srcId);
                    }
                    else
                    {
                        std::cerr << "could not find "
                                  << parameter.second.getSource().value().uniqueName << " ("
                                  << parameter.second.getSource().value().portId
                                  << ")in m_output_ports\n";
                        m_graph = graph::AdjacencyListDirectedGraph(m_lastId);
                        return m_graph;
                    }
                }
            }
        }
        m_allInputReferencesAreValid = true;
        return m_graph;
    }

    void Model::registerOutputs(NodeBase & node)
    {
        for (auto & outPort : node.getOutputs())
        {
            registerOutput(outPort.second);
        }
    }

    void Model::registerInputs(NodeBase & node)
    {
        for (auto & inputPort : node.parameter())
        {
            registerInput(inputPort.second);
        }
    }

    size_t Model::getSize() const
    {
        return m_nodes.size();
    }

    bool Model::updateTypes()
    {
        bool isValid = true;

        for (auto & [id, node] : m_nodes)
        {
            bool nodeIsValid = node->updateTypes(*this);
            if (!nodeIsValid)
            {
                if (m_logger)
                {
                    m_logger->addEvent(
                      events::Event{"For node " + node->getDisplayName() +
                                      " no matching combination of the requested inputs or "
                                      "output (types) could be found \n",
                                    events::Severity::Error});
                }
            }
            isValid = isValid && nodeIsValid;
        }
        return isValid;
    }

    bool Model::isValid()
    {
        return m_isValid;
    }

    void Model::updateValidityState()
    {
        // Reset to true initially, then accumulate validation results
        m_isValid = true;

        if (!m_allInputReferencesAreValid)
        {
            if (m_logger)
            {
                m_logger->addEvent(
                  events::Event{"Not all input references are valid", events::Severity::Error});
            }
            m_isValid = false;
            return;
        }

        if (graph::isCyclic(m_graph))
        {
            if (m_logger)
            {
                m_logger->addEvent(events::Event{"Graph is cyclic", events::Severity::Error});
            }
            m_isValid = false;
            return;
        }

        m_isValid = updateTypes();
    }

    void Model::setDisplayName(std::string const & name)
    {
        m_displayName = name;
    }

    std::optional<std::string> Model::getDisplayName() const
    {
        return m_displayName;
    }

    void Model::setLogger(events::SharedLogger logger)
    {
        m_logger = logger;
    }

    void Model::setResourceId(ResourceId resourceId)
    {
        m_resourceId = resourceId;
        m_name = fmt::format("function_{}", m_resourceId);
    }

    ResourceId Model::getResourceId() const
    {
        return m_resourceId;
    }

    Ports & Model::getInputs()
    {
        if (m_beginNode == nullptr)
        {
            createBeginEnd();
        }
        return m_beginNode->getOutputs();
    }

    ParameterMap & Model::getOutputs()
    {
        if (m_endNode == nullptr)
        {
            createBeginEnd();
        }
        return m_endNode->parameter();
    }

    void Model::setManaged(bool managed)
    {
        m_isManaged = managed;
    }

    bool Model::isManaged() const
    {
        return m_isManaged;
    }

    std::string Model::getSourceName(PortId portId)
    {
        auto portIter = m_outPorts.find(portId);
        if (portIter == std::end(m_outPorts))
        {
            return {};
        }

        const auto sourceNode = getNode(portIter->second->getParentId());
        if (!sourceNode.has_value())
        {
            return {};
        }

        auto sourceName =
          (sourceNode.value() == getBeginNode()) ? "inputs" : sourceNode.value()->getUniqueName();
        sourceName += ".";
        sourceName += portIter->second->getShortName();
        return sourceName;
    }

    Port * Model::getPort(PortId portId)
    {
        auto portIter = m_outPorts.find(portId);
        if (portIter == std::end(m_outPorts))
        {
            return nullptr;
        }
        return portIter->second;
    }

    void Model::invalidateGraph()
    {
        m_graphRequiresUpdate = true;
    }

    void Model::markAsLayouted()
    {
        m_nodesHaveBeenLayouted = true;
    }

    bool Model::hasBeenLayouted() const
    {
        return m_nodesHaveBeenLayouted;
    }

    void Model::setIsValid(bool isValid)
    {
        m_isValid = isValid;
    }

    /**
     * @brief Simplifies the model by removing nodes that are not connected to the end node.
     *
     * This method identifies all nodes that cannot influence the end node (i.e.,
     * there is no path from these nodes to the end node) and removes them.
     * This helps optimize the model by eliminating unused nodes.
     *
     * @return The number of nodes removed during simplification
     */
    size_t Model::simplifyModel()
    {
        // Make sure the graph is up-to-date
        updateGraphAndOrderIfNeeded();

        // If there's no end node, there's nothing to simplify
        if (!m_endNode)
        {
            return 0;
        }

        // Get the node ID of the end node
        NodeId const endNodeId = m_endNode->getId();

        // Create a set of all nodes that are needed (directly or indirectly contribute to the end
        // node)
        std::set<NodeId> neededNodes;

        // First, include the end node itself
        neededNodes.insert(endNodeId);

        // Then add all nodes that the end node depends on
        // We need to carefully handle the dependencies
        if (m_graph.getSize() > 0 && endNodeId < static_cast<NodeId>(m_graph.getSize()))
        {
            auto endNodeDependencies = graph::determineAllDependencies(m_graph, endNodeId);
            neededNodes.insert(endNodeDependencies.begin(), endNodeDependencies.end());
        }

        // Always include the Begin node if it exists
        if (m_beginNode)
        {
            neededNodes.insert(m_beginNode->getId());
        }

        // Find nodes to remove (those not in the needed set)
        std::vector<NodeId> nodesToRemove;
        for (const auto & [nodeId, node] : m_nodes)
        {
            // Skip begin and end nodes (redundant check, but for clarity)
            if (nodeId == endNodeId || (m_beginNode && nodeId == m_beginNode->getId()))
            {
                continue;
            }

            if (neededNodes.find(nodeId) == neededNodes.end())
            {
                nodesToRemove.push_back(nodeId);
            }
        }

        // Remove the unneeded nodes
        size_t removedCount = nodesToRemove.size();
        for (auto nodeId : nodesToRemove)
        {
            remove(nodeId);
        }

        // Update the graph after removing nodes
        m_graphRequiresUpdate = true;
        updateGraphAndOrderIfNeeded();

        return removedCount;

        return removedCount;
    }

    void Model::clear()
    {
        // Clear all nodes and registries
        m_nodes.clear();
        m_outPorts.clear();
        m_inputParameter.clear();

        // Reset node pointers
        m_beginNode = nullptr;
        m_endNode = nullptr;

        // Reset IDs
        m_lastParameterId = 0;
        m_lastId = 1;

        // Reset graph
        m_graph = graph::AdjacencyListDirectedGraph(0);
        m_outputOrder.clear();
        m_graphRequiresUpdate = true;

        // Reset state flags
        m_allInputReferencesAreValid = false;
        m_nodesHaveBeenLayouted = false;
        m_isValid = true;
    }

} // namespace gladius::nodes
