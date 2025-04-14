#pragma once

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

#include "DerivedNodes.h"
#include "EventLogger.h"
#include "Parameter.h"
#include "Port.h"
#include "graph/DirectedGraph.h"
#include "graph/GraphAlgorithms.h"
#include "nodesfwd.h"

namespace gladius::nodes
{
    using NodeRegistry = std::map<int, std::unique_ptr<NodeBase>>;
    using PortRegistry = std::unordered_map<int, Port *>;
    using InputParameterRegistry = std::unordered_map<ParameterId, IParameter *>;
    using SharedAssembly = std::shared_ptr<Assembly>;
    NodeBase * createNodeFromName(const std::string & name, Model & nodes);

    class Model
    {
      public:
        Model() = default;
        Model(Model const & other);

        Model & operator=(Model const & other)
        {
            if (this != &other)
            {
                this->Model::~Model();
                new (this) Model(other);
            }
            return *this;
        };

        Model(Model &&) = default;
        Model & operator=(Model && other) = default;
        ~Model() = default;

        void createBeginEnd();
        void createBeginEndWithDefaultInAndOuts();
        void createValidVoid();

        auto begin()
        {
            return m_nodes.begin();
        }

        auto end()
        {
            return m_nodes.end();
        }

        template <typename NodeType>
        auto create() -> NodeType *
        {
            m_graphRequiresUpdate = true;

            while (m_nodes.find(m_lastId) != m_nodes.end())
            {
                ++m_lastId;
            }

            auto newNode = std::make_unique<NodeType>(m_lastId);

            NodeType * node = newNode.get();
            auto fakeId = m_lastId;
            while (isNodeNameOccupied(node->getUniqueName()))
            {
                ++fakeId;
                node->setUniqueName(NodeName(m_name + "_" + std::to_string(fakeId)));
            }
            node->setDisplayName(node->getUniqueName());
            m_nodes[m_lastId] = std::move(newNode);
            ++m_lastId;

            registerOutputs(*node);
            registerInputs(*node);
            return node;
        }

        template <typename NodeType>
        NodeType * create(NodeType &)
        {
            if constexpr (std::is_same<NodeType, Begin>::value)
            {
                m_beginNode = create<Begin>();
                return m_beginNode;
            }
            else if constexpr (std::is_same<NodeType, End>::value)
            {
                m_endNode = create<End>();
                return m_endNode;
            }
            else
            {
                return create<NodeType>();
            }
        }

        void remove(NodeId id);

        /// Faster remove, but does not update the graph
        void removeNodeWithoutLinks(NodeId idOfNodeWithoutLinks);

        NodeBase * insert(std::unique_ptr<NodeBase> node)
        {
            m_graphRequiresUpdate = true;

            while (m_nodes.find(m_lastId) != m_nodes.end())
            {
                ++m_lastId;
            }

            auto fakeId = m_lastId;
            while (isNodeNameOccupied(node->getUniqueName()))
            {
                ++fakeId;
                node->setUniqueName(NodeName(m_name + "_" + node->getUniqueName() +std::to_string(fakeId)));
            }
            node->setId(m_lastId);

            m_nodes[m_lastId] = std::move(node);
            auto * createdNode = m_nodes.at(m_lastId).get();
            ++m_lastId;

            // reset all input and output ids
            for (auto & outPort : createdNode->getOutputs())
            {
                outPort.second.setId(0);
                outPort.second.setParent(createdNode);
            }

            for (auto & inputPort : createdNode->parameter())
            {
                inputPort.second.setId(0);
                inputPort.second.setParentId(createdNode->getId());
            }

            registerOutputs(*createdNode);
            registerInputs(*createdNode);
            return createdNode;
        }

        /**
         * \brief Test if a link would be valid (compatible types, no circular dependency
         * \param source
         * \param target
         * \return
         * \note Might update the graph (that is why the method is not const)
         */
        [[nodiscard]] bool isLinkValid(Port & source, VariantParameter & target);

        [[nodiscard]] bool isLinkValid(PortId const sourceId, ParameterId const targetId);

        auto addLink(PortId const start, ParameterId const end, bool skipCheck = false) -> bool;

        auto removeLink(PortId const startId, ParameterId const endId) -> bool;

        void visitNodes(Visitor & visitor);

  

        void updateGraphAndOrderIfNeeded();

        [[nodiscard]] std::optional<NodeBase *> getNode(NodeId id) const;

        [[nodiscard]] InputParameterRegistry & getParameterRegistry();

        InputParameterRegistry const & getConstParameterRegistry() const;

        [[nodiscard]] graph::DirectedGraph const & getGraph() const;

        auto getPortRegistry() -> PortRegistry &;

        void addArgument(ParameterName name, VariantParameter parameter);

        void addFunctionOutput(ParameterName name, VariantParameter parameter);

        nodes::Begin * getBeginNode();

        nodes::End * getEndNode();

        void updatePartArguments(NodeId partNodeId,
                                 Model & referencedModel,
                                 std::string const & partParameterName);

        void setModelName(ModelName const & modelName);

        [[nodiscard]] std::string & getModelName();

        void registerInput(VariantParameter & parameter);

        void registerOutput(Port & port);

        [[nodiscard]] bool isNodeNameOccupied(NodeName const & name);
        [[nodiscard]] NodeBase * findNode(NodeName const & name);
        void registerOutputs(NodeBase & node);

        void registerInputs(NodeBase & node);

        size_t getSize() const;

        bool updateTypes();
        [[nodiscard]] bool isValid(); // Not const because it might update the graph
        void updateValidityState(); // Updates the m_isValid state based on the graph validation

        void setDisplayName(std::string const & name);
        [[nodiscard]] std::optional<std::string> getDisplayName() const;

        void setLogger(events::SharedLogger logger);

        void setResourceId(ResourceId resourceId);
        [[nodiscard]] ResourceId getResourceId() const;

        Ports & getInputs();
        ParameterMap & getOutputs();

        void setManaged(bool managed);
        [[nodiscard]] bool isManaged() const;

        std::string getSourceName(PortId portId);

        Port * getPort(PortId portId);

        void invalidateGraph();

        void markAsLayouted();
        [[nodiscard]] bool hasBeenLayouted() const;

        void setIsValid(bool isValid);

        /**
         * @brief Clears the Model, resetting it to its initial state.
         */
        void clear();
        
      private:
        void updateOrder();
        auto buildGraph() -> graph::DirectedGraph &;

        NodeRegistry m_nodes;
        PortRegistry m_outPorts;
        InputParameterRegistry m_inputParameter;
        Begin * m_beginNode{nullptr};
        End * m_endNode{nullptr};

        ParameterId m_lastParameterId{};

        NodeId m_lastId = {1};

        graph::DirectedGraph m_graph{0};
        graph::VertexList m_outputOrder;
        bool m_graphRequiresUpdate = true;

        ModelName m_name{"unnamed"};
        std::optional<std::string> m_displayName;

        events::SharedLogger m_logger;

        ResourceId m_resourceId{0};

        bool m_isManaged = false;
        bool m_allInputReferencesAreValid = false;
        bool m_nodesHaveBeenLayouted = false;

        bool m_isValid = true;
    };

    using SharedModel = std::shared_ptr<Model>;
    using UniqueModel = std::unique_ptr<Model>;

    template <int First, int Last>
    struct static_for
    {
        template <typename Fn>
        void operator()(Fn const & fn) const
        {
            if (First < Last)
            {
                fn(First);
                static_for<First + 1, Last>()(fn);
            }
        }
    };

    template <class Tup, class Func, std::size_t... Is>
    constexpr void static_for_impl(Tup && t, Func && f, std::index_sequence<Is...> /*unused*/)
    {
        (f(std::integral_constant<std::size_t, Is>{}, std::get<Is>(t)), ...);
    }

    template <class... T, class Func>
    constexpr void staticFor(std::tuple<T...> & t, Func && f)
    {
        static_for_impl(t, std::forward<Func>(f), std::make_index_sequence<sizeof...(T)>{});
    }

    void printNodeTypes();
} // namespace gladius::nodes
