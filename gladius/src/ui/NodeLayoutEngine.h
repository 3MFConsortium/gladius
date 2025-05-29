#pragma once

#include "nodes/NodeBase.h"
#include "nodes/Model.h" 
#include "nodes/graph/GraphAlgorithms.h"
#include <map>
#include <vector>
#include <unordered_map>
#include <string>

namespace gladius::ui
{
    /**
     * @brief Dedicated engine for performing automatic layout of nodes with group awareness.
     * 
     * This class encapsulates all auto layout functionality, making it reusable across different
     * contexts. It provides sophisticated group-aware layout algorithms that ensure:
     * - Groups don't overlap with other groups
     * - Ungrouped nodes stay outside group boundaries
     * - Topological ordering is maintained
     * - Links are kept as short as possible
     */
    class NodeLayoutEngine
    {
    public:
        /**
         * @brief Configuration parameters for the layout algorithm
         */
        struct LayoutConfig
        {
            float nodeDistance = 200.0f;                    ///< Basic distance between nodes
            float groupSpacingMultiplier = 1.5f;           ///< Extra spacing factor between groups
            float minGroupSpacing = 40.0f;                 ///< Minimum spacing between group boundaries
            int maxOverlapResolutionIterations = 10;       ///< Max iterations for overlap resolution
            int maxCrossingReductionIterations = 5;        ///< Max iterations for crossing reduction
            float crossingReductionConvergenceThreshold = 5.0f; ///< Threshold for crossing reduction convergence
            float linkDistanceWeight = 1.0f;               ///< Weight for link distance optimization
        };

        /**
         * @brief Information about group depth constraints for layout optimization
         */
        struct GroupDepthInfo
        {
            std::string tag;                                ///< Group identifier
            int minRequiredDepth;                          ///< Minimum depth based on dependencies
            int maxRequiredDepth;                          ///< Maximum depth based on dependencies
            std::vector<nodes::NodeId> nodeIds;           ///< Nodes in this group
            bool canBeMovedTogether;                       ///< Whether group can be placed as unit
        };

        /**
         * @brief Represents an optimized placement choice for a group
         */
        struct GroupPlacementOption
        {
            std::string tag;                               ///< Group identifier
            int chosenDepth;                               ///< Selected depth for placement
            float placementCost;                           ///< Cost metric for this placement
        };

        /**
         * @brief Performs automatic layout of nodes in the given model
         * 
         * @param model The node model to layout
         * @param config Layout configuration parameters
         */
        void performAutoLayout(nodes::Model& model, const LayoutConfig& config);

    private:
        /**
         * @brief Analyzes depth constraints for all groups in the model
         * 
         * @param depthMap Mapping from node ID to its topological depth
         * @return Vector of group depth information
         */
        std::vector<GroupDepthInfo> analyzeGroupDepthConstraints(
            const std::unordered_map<nodes::NodeId, int>& depthMap,
            nodes::Model& model);

        /**
         * @brief Optimizes placement of groups to minimize fragmentation
         * 
         * @param groupInfos Information about group constraints
         * @param layers Current layer assignments
         * @return Optimized placement choices for each group
         */
        std::vector<GroupPlacementOption> optimizeGroupPlacements(
            const std::vector<GroupDepthInfo>& groupInfos,
            std::map<int, std::vector<nodes::NodeBase*>>& layers);

        /**
         * @brief Applies group-aware coordinate assignment to nodes
         * 
         * @param layers Map of depth to nodes in that layer
         * @param layerXPositions X coordinates for each layer
         * @param originalIndividualDepths Original topological depths for sorting within groups
         * @param config Layout configuration
         */
        void applyGroupAwareCoordinates(
            std::map<int, std::vector<nodes::NodeBase*>>& layers,
            const std::map<int, float>& layerXPositions,
            const std::unordered_map<nodes::NodeId, int>& originalIndividualDepths,
            const LayoutConfig& config);

        /**
         * @brief Resolves overlaps between group visual boundaries
         * 
         * @param layers Map of depth to nodes in that layer
         * @param config Layout configuration
         */
        void resolveGroupOverlaps(
            std::map<int, std::vector<nodes::NodeBase*>>& layers,
            const LayoutConfig& config);

        /**
         * @brief Calculates placement cost for a group at a specific depth
         * 
         * @param groupInfo Information about the group
         * @param targetDepth Proposed depth for placement
         * @param layers Current layer assignments
         * @return Cost metric (lower is better)
         */
        float calculateGroupPlacementCost(
            const GroupDepthInfo& groupInfo,
            int targetDepth,
            const std::map<int, std::vector<nodes::NodeBase*>>& layers);

        /**
         * @brief Helper to determine node topological depth from dependencies
         * 
         * @param graph The node graph
         * @param beginId ID of the starting node
         * @return Map from node ID to depth
         */
        std::unordered_map<nodes::NodeId, int> determineDepth(
            const nodes::graph::IDirectedGraph& graph,
            nodes::NodeId beginId);

        /**
         * @brief Optimizes Y positions to reduce crossings and minimize link distances
         * 
         * @param layers Map of depth to nodes in that layer
         * @param model Node model for accessing graph connectivity
         * @param config Layout configuration
         */
        void optimizeYPositions(
            std::map<int, std::vector<nodes::NodeBase*>>& layers,
            nodes::Model& model,
            const LayoutConfig& config);

        /**
         * @brief Calculates barycenter Y position for a node based on its neighbors
         * 
         * @param node Target node
         * @param layers All layers with nodes
         * @param model Node model for accessing connectivity
         * @return Calculated barycenter Y position
         */
        float calculateBarycenter(
            nodes::NodeBase* node,
            const std::map<int, std::vector<nodes::NodeBase*>>& layers,
            nodes::Model& model) const;

        /**
         * @brief Counts edge crossings between two adjacent layers
         * 
         * @param leftLayer Nodes in the left layer
         * @param rightLayer Nodes in the right layer
         * @param model Node model for accessing connectivity
         * @return Number of edge crossings
         */
        int countCrossings(
            const std::vector<nodes::NodeBase*>& leftLayer,
            const std::vector<nodes::NodeBase*>& rightLayer,
            nodes::Model& model) const;

        /**
         * @brief Calculates total weighted link distance for all edges
         * 
         * @param layers All layers with nodes
         * @param model Node model for accessing connectivity
         * @return Total weighted link distance
         */
        float calculateTotalLinkDistance(
            const std::map<int, std::vector<nodes::NodeBase*>>& layers,
            nodes::Model& model) const;

        /**
         * @brief Updates group bounds after Y position changes
         * 
         * @param layers All layers with nodes
         * @param config Layout configuration
         */
        void updateGroupBounds(
            const std::map<int, std::vector<nodes::NodeBase*>>& layers,
            const LayoutConfig& config);

        /**
         * @brief Checks if a node position violates group boundary constraints
         * 
         * @param node Node to check
         * @param newY Proposed Y position
         * @param layers All layers with nodes
         * @return True if position is valid
         */
        bool isPositionValid(
            nodes::NodeBase* node,
            float newY,
            const std::map<int, std::vector<nodes::NodeBase*>>& layers) const;

        /**
         * @brief Optimizes Y positions for nodes in a single layer
         * 
         * @param layerNodes Nodes in the layer to optimize
         * @param layers All layers with nodes
         * @param model Node model for accessing connectivity
         * @param config Layout configuration
         * @return True if positions were changed
         */
        bool optimizeLayerYPositions(
            std::vector<nodes::NodeBase*>& layerNodes,
            const std::map<int, std::vector<nodes::NodeBase*>>& layers,
            nodes::Model& model,
            const LayoutConfig& config);
    };

} // namespace gladius::ui
