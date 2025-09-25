#pragma once

#include "nodes/Model.h"
#include "nodes/NodeBase.h"
#include "nodes/graph/GraphAlgorithms.h"
#include <imgui.h>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace gladius::ui
{
    /**
     * @brief Dedicated engine for performing automatic layout of nodes with group awareness.
     *
     * This class encapsulates all auto layout functionality using a generic layered layout
     * algorithm. It ensures:
     * - Groups don't overlap with other groups
     * - Ungrouped nodes stay outside group boundaries
     * - Topological ordering is maintained
     * - Constant nodes are placed close to their connected nodes
     * - Consistent structure with nodes arranged in dependency-based layers
     */
    class NodeLayoutEngine
    {
      public:
        /**
         * @brief Configuration for layout algorithm
         */
        struct LayoutConfig
        {
            float nodeDistance = 200.0f;         ///< Basic distance between nodes
            float layerSpacing = 500.0f;         ///< Distance between layers
            float groupPadding = 200.0f;         ///< Padding around groups
            float constantNodeOffset = 80.0f;    ///< Offset for constant nodes from their targets
            int maxOptimizationIterations = 100; ///< Max iterations for position optimization
            float convergenceThreshold = 5.0f;   ///< Threshold for optimization convergence
        };

        /**
         * @brief Rectangle structure for tracking occupied spaces
         */
        struct Rect
        {
            ImVec2 min; ///< Top-left corner
            ImVec2 max; ///< Bottom-right corner

            Rect()
                : min(0.0f, 0.0f)
                , max(0.0f, 0.0f)
            {
            }
            Rect(const ImVec2 & minPt, const ImVec2 & maxPt)
                : min(minPt)
                , max(maxPt)
            {
            }

            bool overlaps(const Rect & other) const
            {
                return max.x > other.min.x && min.x < other.max.x && max.y > other.min.y &&
                       min.y < other.max.y;
            }
        };

        /**
         * @brief Generic layout entity that can represent nodes, groups, or other layout units
         */
        template <typename T>
        struct LayoutEntity
        {
            T * item;                                    ///< The actual item being laid out
            int depth;                                   ///< Topological depth/layer
            ImVec2 position;                             ///< Screen position
            ImVec2 size;                                 ///< Size of the entity
            std::vector<LayoutEntity<T> *> dependencies; ///< What this entity depends on
            std::vector<LayoutEntity<T> *> dependents;   ///< What depends on this entity

            LayoutEntity(T * item_, int depth_)
                : item(item_)
                , depth(depth_)
                , position(0.0f, 0.0f)
                , size(0.0f, 0.0f)
            {
            }
        };

        using NodeEntity = LayoutEntity<nodes::NodeBase>;
        using GroupEntity = LayoutEntity<std::vector<nodes::NodeBase *>>;

        /**
         * @brief Information about a group for layout purposes
         */
        struct GroupInfo
        {
            std::string name;                     ///< Group name/tag
            std::string tag;                      ///< Group tag (same as name, for compatibility)
            std::vector<nodes::NodeBase *> nodes; ///< Nodes in this group
            int depth;                            ///< Group depth (min depth of contained nodes)
            int minDepth;                         ///< Minimum depth of contained nodes
            int maxDepth;                         ///< Maximum depth of contained nodes
            ImVec2 position;                      ///< Group position
            ImVec2 size;                          ///< Group size

            GroupInfo()
                : depth(0)
                , minDepth(0)
                , maxDepth(0)
                , position(0.0f, 0.0f)
                , size(0.0f, 0.0f)
            {
            }
        };

        /**
         * @brief Main auto-layout method
         *
         * Performs complete auto-layout of all nodes in the model, handling:
         * - Grouped vs ungrouped nodes
         * - Topological ordering
         * - Group boundaries and spacing
         * - Constant node optimal positioning
         * - Overlap resolution
         *
         * @param model The node model to layout
         * @param config Layout configuration parameters
         */
        void performAutoLayout(nodes::Model & model, const LayoutConfig & config);

        /**
         * @brief Generic layered layout algorithm
         *
         * This template method can layout any type of entity (nodes, groups, etc.)
         * using dependency-based layers and topological sorting.
         *
         * @tparam T The type of entity being laid out
         * @param entities Vector of entities to layout
         * @param config Layout configuration
         */
        template <typename T>
        void performLayeredLayout(std::vector<LayoutEntity<T>> & entities,
                                  const LayoutConfig & config,
                                  const std::vector<Rect> & occupiedRects = {});

        /**
         * @brief Check if a node is a constant node (ConstantScalar, ConstantVector,
         * ConstantMatrix)
         *
         * @param node Node to check
         * @return true if the node is a constant node
         */
        bool isConstantNode(nodes::NodeBase * node) const;

      private:
        /**
         * @brief Analyze groups and create GroupInfo structures
         *
         * @param model The node model
         * @param depthMap Node depth mapping
         * @return Vector of group information
         */
        std::vector<GroupInfo>
        analyzeGroups(nodes::Model & model,
                      const std::unordered_map<nodes::NodeId, int> & depthMap);

        /**
         * @brief Layout ungrouped nodes using the generic algorithm
         *
         * @param ungroupedNodes Nodes not belonging to any group
         * @param depthMap Node depth mapping
         * @param config Layout configuration
         */
        void layoutUngroupedNodes(const std::vector<nodes::NodeBase *> & ungroupedNodes,
                                  const std::unordered_map<nodes::NodeId, int> & depthMap,
                                  const LayoutConfig & config,
                                  const std::vector<Rect> & occupiedRects = {});

        /**
         * @brief Layout nodes within a specific group
         *
         * @param groupInfo Group information with nodes to layout
         * @param depthMap Node depth mapping
         * @param config Layout configuration
         */
        void layoutNodesInGroup(GroupInfo & groupInfo,
                                const std::unordered_map<nodes::NodeId, int> & depthMap,
                                const LayoutConfig & config,
                                const std::vector<Rect> & occupiedRects = {});

        /**
         * @brief Layout groups themselves to avoid overlaps
         *
         * @param groups Vector of group information
         * @param config Layout configuration
         */
        void layoutGroups(std::vector<GroupInfo> & groups, const LayoutConfig & config);

        /**
         * @brief Helper to determine node topological depth from dependencies
         *
         * @param graph The dependency graph
         * @param beginId Starting node ID
         * @return Map of node ID to depth
         */
        std::unordered_map<nodes::NodeId, int>
        determineDepth(const nodes::graph::IDirectedGraph & graph, nodes::NodeId beginId);

        /**
         * @brief Correct depth for constant nodes with depth 0 (excluding begin node)
         *
         * Constant nodes that have depth 0 are placed in the same layer as the begin node.
         * This method corrects their depth by setting it to the minimum depth of their
         * connected/consuming nodes minus 1, placing them in the layer just before
         * their consumers.
         *
         * @param model The node model
         * @param graph The dependency graph
         * @param depthMap The depth map to modify
         * @param beginId The begin node ID (excluded from correction)
         */
        void correctConstantNodeDepths(nodes::Model & model,
                                       const nodes::graph::IDirectedGraph & graph,
                                       std::unordered_map<nodes::NodeId, int> & depthMap,
                                       nodes::NodeId beginId);

        /**
         * @brief Arrange entities into layers by depth
         *
         * @tparam T Entity type
         * @param entities Vector of entities
         * @return Map of depth to entities at that depth
         */
        template <typename T>
        std::map<int, std::vector<LayoutEntity<T> *>>
        arrangeInLayers(std::vector<LayoutEntity<T>> & entities);

        /**
         * @brief Optimize positions within layers to minimize edge crossings
         *
         * @tparam T Entity type
         * @param layers Map of depth to entities
         * @param config Layout configuration
         */
        template <typename T>
        void optimizeLayerPositions(std::map<int, std::vector<LayoutEntity<T> *>> & layers,
                                    const LayoutConfig & config);

        /**
         * @brief Optimize a single layer by ordering nodes based on their connections
         *
         * @tparam T Entity type
         * @param layerEntities Entities in the current layer
         * @param allLayers All layers for connection lookup
         * @param currentDepth Current layer depth
         * @param config Layout configuration
         */
        template <typename T>
        void optimizeLayerByConnectionOrder(
          std::vector<LayoutEntity<T> *> & layerEntities,
          const std::map<int, std::vector<LayoutEntity<T> *>> & allLayers,
          int currentDepth,
          const LayoutConfig & config);

        /**
         * @brief Calculate average Y position of connected nodes
         *
         * @tparam T Entity type
         * @param entity Entity to calculate for
         * @param allLayers All layers for connection lookup
         * @param currentDepth Current layer depth
         * @param config Layout configuration
         * @return Average Y position of connected nodes
         */
        template <typename T>
        float
        calculateAverageConnectedY(LayoutEntity<T> * entity,
                                   const std::map<int, std::vector<LayoutEntity<T> *>> & allLayers,
                                   int currentDepth,
                                   const LayoutConfig & config);

        /**
         * @brief Check if two nodes are connected via ports
         *
         * @tparam T Node type
         * @param node1 First node
         * @param node2 Second node
         * @return true if nodes are connected
         */
        template <typename T>
        bool areNodesConnected(T * node1, T * node2);

        /**
         * @brief Calculate size of an entity
         *
         * @param entity Entity to measure
         * @return Size of the entity
         */
        ImVec2 calculateEntitySize(NodeEntity & entity);

        /**
         * @brief Update group bounds based on contained nodes
         *
         * @param groupInfo Group to update
         */
        void updateGroupBounds(GroupInfo & groupInfo);

        /**
         * @brief Calculate group size based on contained nodes
         *
         * @param groupInfo Group information
         * @return Calculated group size
         */
        ImVec2 calculateGroupSize(const GroupInfo & groupInfo);

        /**
         * @brief Optimize positions in a single layer to minimize edge lengths
         *
         * @tparam T Entity type
         * @param layerEntities Entities in the layer to optimize
         * @param allLayers All layers for connection lookup
         * @param currentDepth Current layer depth
         * @param config Layout configuration
         */
        template <typename T>
        void optimizeSingleLayer(std::vector<LayoutEntity<T> *> & layerEntities,
                                 const std::map<int, std::vector<LayoutEntity<T> *>> & allLayers,
                                 int currentDepth,
                                 const LayoutConfig & config);

        /**
         * @brief Group entities by their tag for group-aware optimization
         *
         * @param entities Input entities
         * @param groups Output grouped entities
         * @param ungrouped Output ungrouped entities
         */
        void groupEntitiesByTag(const std::vector<LayoutEntity<nodes::NodeBase> *> & entities,
                                std::vector<std::vector<LayoutEntity<nodes::NodeBase> *>> & groups,
                                std::vector<LayoutEntity<nodes::NodeBase> *> & ungrouped);

        /**
         * @brief Optimization unit for group-aware positioning
         */
        template <typename T>
        struct OptimizationUnit
        {
            std::vector<LayoutEntity<T> *> entities;
            float minY, maxY;

            explicit OptimizationUnit(std::vector<LayoutEntity<T> *> entities_);
            void updateBounds();
            float getHeight() const;
            float getCenterY() const;
        };

        /**
         * @brief Calculate center Y position of an optimization unit
         */
        template <typename T>
        float calculateUnitCenterY(const OptimizationUnit<T> & unit);

        /**
         * @brief Calculate optimal Y position for a unit based on connections
         */
        template <typename T>
        float
        calculateOptimalYPosition(const OptimizationUnit<T> & unit,
                                  const std::map<int, std::vector<LayoutEntity<T> *>> & allLayers,
                                  int currentDepth,
                                  const LayoutConfig & config);

        /**
         * @brief Get connections for a node in adjacent layers
         */
        template <typename T>
        std::vector<std::pair<LayoutEntity<T> *, float>>
        getNodeConnections(T * node,
                           const std::map<int, std::vector<LayoutEntity<T> *>> & allLayers,
                           int currentDepth);

        /**
         * @brief Move an optimization unit to a target Y position
         */
        template <typename T>
        void moveOptimizationUnit(OptimizationUnit<T> & unit, float targetTopY);

        /**
         * @brief Resolve overlaps between units in a layer
         */
        template <typename T>
        void resolveLayerOverlaps(std::vector<OptimizationUnit<T>> & units,
                                  const LayoutConfig & config);
    };
}
