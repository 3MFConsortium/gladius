#pragma once

#include "nodes/NodeBase.h"
#include "nodes/Model.h" 
#include "nodes/graph/GraphAlgorithms.h"
#include <imgui.h>
#include <map>
#include <vector>
#include <unordered_map>
#include <string>

namespace gladius::ui
{
    /**
     * @brief Dedicated engine for performing automatic layout of nodes with group awareness.
     * 
     * This class encapsulates all auto layout functionality using a generic layered layout algorithm.
     * It ensures:
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
            float nodeDistance = 150.0f;                    ///< Basic distance between nodes
            float layerSpacing = 250.0f;                   ///< Distance between layers
            float groupPadding = 50.0f;                    ///< Padding around groups
            float constantNodeOffset = 80.0f;              ///< Offset for constant nodes from their targets
            int maxOptimizationIterations = 10;            ///< Max iterations for position optimization
            float convergenceThreshold = 5.0f;             ///< Threshold for optimization convergence
        };

        /**
         * @brief Generic layout entity that can represent nodes, groups, or other layout units
         */
        template<typename T>
        struct LayoutEntity
        {
            T* item;                                        ///< The actual item being laid out
            int depth;                                      ///< Topological depth/layer
            ImVec2 position;                               ///< Screen position
            ImVec2 size;                                   ///< Size of the entity
            std::vector<LayoutEntity<T>*> dependencies;    ///< What this entity depends on
            std::vector<LayoutEntity<T>*> dependents;      ///< What depends on this entity
            
            LayoutEntity(T* item_, int depth_) : item(item_), depth(depth_), position(0.0f, 0.0f), size(0.0f, 0.0f) {}
        };

        using NodeEntity = LayoutEntity<nodes::NodeBase>;
        using GroupEntity = LayoutEntity<std::vector<nodes::NodeBase*>>;

        /**
         * @brief Information about a group for layout purposes
         */
        struct GroupInfo
        {
            std::string name;                              ///< Group name/tag
            std::string tag;                               ///< Group tag (same as name, for compatibility)
            std::vector<nodes::NodeBase*> nodes;           ///< Nodes in this group
            int depth;                                     ///< Group depth (min depth of contained nodes)
            int minDepth;                                  ///< Minimum depth of contained nodes
            int maxDepth;                                  ///< Maximum depth of contained nodes
            ImVec2 position;                               ///< Group position
            ImVec2 size;                                   ///< Group size
            
            GroupInfo() : depth(0), minDepth(0), maxDepth(0), position(0.0f, 0.0f), size(0.0f, 0.0f) {}
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
        void performAutoLayout(nodes::Model& model, const LayoutConfig& config);

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
        template<typename T>
        void performLayeredLayout(std::vector<LayoutEntity<T>>& entities, const LayoutConfig& config);

        /**
         * @brief Check if a node is a constant node (ConstantScalar, ConstantVector, ConstantMatrix)
         * 
         * @param node Node to check
         * @return true if the node is a constant node
         */
        bool isConstantNode(nodes::NodeBase* node) const;

        /**
         * @brief Calculate optimal position for a constant node
         * 
         * Places constant nodes close to their connected nodes, typically to the left
         * of the leftmost connected node.
         * 
         * @param constantNode The constant node to position
         * @param model The node model
         * @param config Layout configuration
         * @return Optimal position for the constant node
         */
        ImVec2 calculateConstantNodePosition(nodes::NodeBase* constantNode, 
                                           nodes::Model& model, 
                                           const LayoutConfig& config);

    private:
        /**
         * @brief Analyze groups and create GroupInfo structures
         * 
         * @param model The node model
         * @param depthMap Node depth mapping
         * @return Vector of group information
         */
        std::vector<GroupInfo> analyzeGroups(nodes::Model& model, 
                                            const std::unordered_map<nodes::NodeId, int>& depthMap);

        /**
         * @brief Layout ungrouped nodes using the generic algorithm
         * 
         * @param ungroupedNodes Nodes not belonging to any group
         * @param depthMap Node depth mapping
         * @param config Layout configuration
         */
        void layoutUngroupedNodes(const std::vector<nodes::NodeBase*>& ungroupedNodes,
                                 const std::unordered_map<nodes::NodeId, int>& depthMap,
                                 const LayoutConfig& config);

        /**
         * @brief Layout nodes within a specific group
         * 
         * @param groupInfo Group information with nodes to layout
         * @param depthMap Node depth mapping
         * @param config Layout configuration
         */
        void layoutNodesInGroup(GroupInfo& groupInfo,
                               const std::unordered_map<nodes::NodeId, int>& depthMap,
                               const LayoutConfig& config);

        /**
         * @brief Layout groups themselves to avoid overlaps
         * 
         * @param groups Vector of group information
         * @param config Layout configuration
         */
        void layoutGroups(std::vector<GroupInfo>& groups, const LayoutConfig& config);

        /**
         * @brief Resolve overlaps between nodes and groups
         * 
         * @param ungroupedNodes Ungrouped nodes
         * @param constantNodes Constant nodes (positioned optimally)
         * @param groups Group information
         * @param config Layout configuration
         */
        void resolveOverlaps(const std::vector<nodes::NodeBase*>& ungroupedNodes,
                           const std::vector<nodes::NodeBase*>& constantNodes,
                           std::vector<GroupInfo>& groups,
                           const LayoutConfig& config);

        /**
         * @brief Helper to determine node topological depth from dependencies
         * 
         * @param graph The dependency graph
         * @param beginId Starting node ID
         * @return Map of node ID to depth
         */
        std::unordered_map<nodes::NodeId, int> determineDepth(
            const nodes::graph::IDirectedGraph& graph,
            nodes::NodeId beginId);

        /**
         * @brief Arrange entities into layers by depth
         * 
         * @tparam T Entity type
         * @param entities Vector of entities
         * @return Map of depth to entities at that depth
         */
        template<typename T>
        std::map<int, std::vector<LayoutEntity<T>*>> arrangeInLayers(std::vector<LayoutEntity<T>>& entities);

        /**
         * @brief Optimize positions within layers to minimize edge crossings
         * 
         * @tparam T Entity type
         * @param layers Map of depth to entities
         * @param config Layout configuration
         */
        template<typename T>
        void optimizeLayerPositions(std::map<int, std::vector<LayoutEntity<T>*>>& layers, const LayoutConfig& config);

        /**
         * @brief Calculate size of an entity
         * 
         * @param entity Entity to measure
         * @return Size of the entity
         */
        ImVec2 calculateEntitySize(NodeEntity& entity);

        /**
         * @brief Update group bounds based on contained nodes
         * 
         * @param groupInfo Group to update
         */
        void updateGroupBounds(GroupInfo& groupInfo);

        /**
         * @brief Calculate group size based on contained nodes
         * 
         * @param groupInfo Group information
         * @return Calculated group size
         */
        ImVec2 calculateGroupSize(const GroupInfo& groupInfo);
    };
}
