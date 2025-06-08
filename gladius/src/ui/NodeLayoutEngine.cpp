#include "NodeLayoutEngine.h"
#include "imgui-node-editor/imgui_node_editor.h"
#include "imgui.h"
#include "nodes/graph/GraphAlgorithms.h"
#include <algorithm>
#include <limits>
#include <unordered_set>

namespace ed = ax::NodeEditor;

namespace gladius::ui
{
    void NodeLayoutEngine::performAutoLayout(nodes::Model & model, const LayoutConfig & config)
    {

        auto const graph = model.getGraph();

        // Special case: exactly two nodes (Begin and End)
        if (model.getSize() == 2 && model.getBeginNode() && model.getEndNode())
        {
            model.getBeginNode()->screenPos() = nodes::float2(0.0f, 0.0f);
            model.getEndNode()->screenPos() = nodes::float2(400.0f, 0.0f);
            model.markAsLayouted();
            return;
        }

        if (graph.getSize() < 2)
        {
            return;
        }

        model.updateGraphAndOrderIfNeeded();

        if (!model.getBeginNode())
        {
            return;
        }

        auto const beginId = model.getBeginNode()->getId();
        auto depthMap = determineDepth(graph, beginId);

        // Step 1: Separate nodes into constant, ungrouped, and identify those in groups
        std::vector<nodes::NodeBase *> constantNodes;
        std::vector<nodes::NodeBase *> ungroupedNodesPreFilter; // Will be filtered later
        std::vector<nodes::NodeBase *> allNonConstantNodes;     // For depth adjustment

        for (auto & [id, node] : model)
        {
            if (isConstantNode(node.get()))
            {
                constantNodes.push_back(node.get());
            }
            else
            {
                allNonConstantNodes.push_back(node.get());
                if (node->getTag().empty())
                {
                    ungroupedNodesPreFilter.push_back(node.get());
                }
            }
        }

        // Step 1.5: Create an adjusted depth map to make space for constant nodes
        auto adjustedDepthMap = depthMap; // Start with a copy
        std::set<int> originalDepthsRequiringShift;

        for (auto * ncNode : allNonConstantNodes)
        {
            auto ncNodeId = ncNode->getId();
            if (depthMap.find(ncNodeId) == depthMap.end())
                continue; // Should not happen for connected graph
            int originalNodeDepth = depthMap[ncNodeId];

            for (auto * cNode : constantNodes)
            {
                if (graph.isDirectlyDependingOn(ncNodeId, cNode->getId()))
                {
                    originalDepthsRequiringShift.insert(originalNodeDepth);
                    break;
                }
            }
        }

        std::map<int, int> accumulatedShiftAtOriginalDepth;
        int currentTotalShift = 0;
        int maxOriginalDepth = 0;
        if (!depthMap.empty())
        {
            for (auto const & [id, dVal] : depthMap)
            {
                maxOriginalDepth = std::max(maxOriginalDepth, dVal);
            }
        }

        for (int d = 0; d <= maxOriginalDepth; ++d)
        {
            if (originalDepthsRequiringShift.count(d))
            {
                currentTotalShift++;
            }
            accumulatedShiftAtOriginalDepth[d] = currentTotalShift;
        }

        for (auto * ncNode : allNonConstantNodes)
        {
            auto ncNodeId = ncNode->getId();
            if (depthMap.find(ncNodeId) != depthMap.end()) // Check if node is in original depth map
            {
                int originalDepth = depthMap[ncNodeId];
                if (accumulatedShiftAtOriginalDepth.count(originalDepth))
                {
                    adjustedDepthMap[ncNodeId] =
                      originalDepth + accumulatedShiftAtOriginalDepth[originalDepth];
                }
                else
                {
                    // This case should ideally not be hit if maxOriginalDepth is calculated
                    // correctly and all non-constant nodes are in depthMap. Default to original
                    // depth if something is off.
                    adjustedDepthMap[ncNodeId] = originalDepth;
                }
            }
        }
        // Constant nodes do not use this depth map for their primary layering.

        // Step 2: Analyze groups using the adjusted depth map
        auto groups = analyzeGroups(model, adjustedDepthMap);

        // Filter ungroupedNodes again, ensuring they are not part of any analyzed group
        // (analyzeGroups only considers nodes with tags for groups)
        std::vector<nodes::NodeBase *> ungroupedNodes;
        for (auto * node : ungroupedNodesPreFilter)
        {
            bool inAGroup = false;
            for (const auto & group : groups)
            {
                if (std::find(group.nodes.begin(), group.nodes.end(), node) != group.nodes.end())
                {
                    inAGroup = true;
                    break;
                }
            }
            if (!inAGroup)
            {
                ungroupedNodes.push_back(node);
            }
        }

        // Step 2.5: Layout each group separately and track occupied spaces
        constexpr float GROUP_PADDING = 50.0f;
        std::vector<Rect> occupiedRects; // Rectangles representing occupied spaces
        ImVec2 nextGroupOrigin(0.0f, 0.0f);
        float maxRowHeight = 0.0f;
        float maxX = 0.0f;

        // Sort groups by minDepth for consistent placement order
        std::sort(groups.begin(),
                  groups.end(),
                  [](const GroupInfo & a, const GroupInfo & b) { return a.minDepth < b.minDepth; });
        for (auto & group : groups)
        { // Layout nodes in group (local coordinates)
            layoutNodesInGroup(
              group, adjustedDepthMap, config, occupiedRects); // Use adjustedDepthMap
            updateGroupBounds(group);

            // Place group at next available position, considering occupied spaces
            // For simplicity, arrange groups in a row with GROUP_PADDING between
            ImVec2 groupOffset = nextGroupOrigin;
            for (auto * node : group.nodes)
            {
                auto & nodePos = node->screenPos();
                nodePos.x += groupOffset.x;
                nodePos.y += groupOffset.y;
            }
            updateGroupBounds(group); // Track occupied rect
            occupiedRects.push_back(
              Rect(group.position,
                   ImVec2(group.position.x + group.size.x, group.position.y + group.size.y)));

            // Update nextGroupOrigin for next group (to the right, with padding)
            nextGroupOrigin.x += group.size.x + GROUP_PADDING;
            maxRowHeight = std::max(maxRowHeight, group.size.y);
            maxX = std::max(maxX, nextGroupOrigin.x);
        }

        // Step 3: Place ungrouped nodes after all groups, considering occupied spaces
        if (!ungroupedNodes.empty()) // Check if there are any truly ungrouped nodes
        {                            // Layout ungrouped nodes (local coordinates)
            layoutUngroupedNodes(
              ungroupedNodes, adjustedDepthMap, config, occupiedRects); // Use adjustedDepthMap

            // Find bounding box of all groups
            float ungroupedOriginX = maxX;
            float ungroupedOriginY = 0.0f;
            if (!occupiedRects.empty())
            {
                bool firstOccupiedRect = true;
                float minOccupiedX = std::numeric_limits<float>::max();
                float maxOccupiedYAtMinX = 0.0f;

                for (const auto & rect : occupiedRects)
                {
                    if (maxX < config.layerSpacing * 2)
                    { // Heuristic: if groups are narrow
                        ungroupedOriginY = std::max(ungroupedOriginY, rect.max.y + GROUP_PADDING);
                    }
                    ungroupedOriginX = std::max(ungroupedOriginX, rect.max.x + GROUP_PADDING);
                }
            }

            // Place all ungrouped nodes, adjusting their local layout positions
            for (auto * node : ungroupedNodes)
            {
                auto & nodePos = node->screenPos();
                nodePos.x += ungroupedOriginX;
                nodePos.y += ungroupedOriginY;
            }

            // Add ungrouped nodes to occupiedRects
            for (auto * node : ungroupedNodes)
            {
                auto nodeSize = ed::GetNodeSize(node->getId());
                if (nodeSize.x <= 0.0f)
                    nodeSize.x = 200.0f; // Default width from calculateEntitySize
                if (nodeSize.y <= 0.0f)
                    nodeSize.y = 100.0f; // Default height from calculateEntitySize

                occupiedRects.push_back(
                  Rect(ImVec2(node->screenPos().x, node->screenPos().y),
                       ImVec2(node->screenPos().x + nodeSize.x, node->screenPos().y + nodeSize.y)));
            }
        }

        // Step 4: Position constant nodes close to their connected nodes and avoid overlaps
        for (auto * constantNode : constantNodes)
        {
            // Get the initial ideal position
            ImVec2 idealPos = calculateConstantNodePosition(constantNode, model, config);
            constantNode->screenPos() = nodes::float2(idealPos.x, idealPos.y); // Start with ideal

            auto nodeSize = ed::GetNodeSize(constantNode->getId());
            if (nodeSize.x <= 0.0f)
                nodeSize.x = 150.0f; // Default size for constant nodes
            if (nodeSize.y <= 0.0f)
                nodeSize.y = 80.0f;

            Rect nodeRect(ImVec2(constantNode->screenPos().x, constantNode->screenPos().y),
                          ImVec2(constantNode->screenPos().x + nodeSize.x,
                                 constantNode->screenPos().y + nodeSize.y));

            bool hasOverlapIteration =
              true; // Assume overlap initially to enter the loop for the first check
            int adjustments = 0;
            const int MAX_ADJUSTMENTS_PER_NODE = 100; // Increased attempts

            while (hasOverlapIteration && adjustments < MAX_ADJUSTMENTS_PER_NODE)
            {
                hasOverlapIteration =
                  false; // Reset for this iteration. Will be set true if an overlap is found.
                adjustments++;

                for (const auto & occRect : occupiedRects)
                {
                    if (nodeRect.overlaps(occRect))
                    {
                        hasOverlapIteration = true; // Found an overlap

                        ImVec2 currentPos =
                          ImVec2(constantNode->screenPos().x, constantNode->screenPos().y);
                        ImVec2 newProposedPos = currentPos;
                        float smallest_displacement_found = std::numeric_limits<float>::max();
                        bool move_found = false;
                        const float padding =
                          config.nodeDistance / 2.0f; // Use a consistent padding

                        // Try moving Right (constant node to the right of occRect)
                        float targetX_R = occRect.max.x + padding;
                        float displacement_R = targetX_R - currentPos.x; // Must be positive
                        if (displacement_R > 1e-4f)
                        { // Must be a real move to the right
                            if (displacement_R < smallest_displacement_found)
                            {
                                smallest_displacement_found = displacement_R;
                                newProposedPos = ImVec2(targetX_R, currentPos.y);
                                move_found = true;
                            }
                        }

                        // Try moving Left (constant node to the left of occRect)
                        float targetX_L = occRect.min.x - nodeSize.x - padding;
                        float displacement_L =
                          currentPos.x - targetX_L; // Must be positive for a leftward move
                        if (displacement_L > 1e-4f)
                        {
                            if (displacement_L < smallest_displacement_found)
                            {
                                smallest_displacement_found = displacement_L;
                                newProposedPos = ImVec2(targetX_L, currentPos.y);
                                move_found = true;
                            }
                        }

                        // Try moving Down (constant node below occRect)
                        float targetY_D = occRect.max.y + padding;
                        float displacement_D = targetY_D - currentPos.y; // Must be positive
                        if (displacement_D > 1e-4f)
                        {
                            if (displacement_D < smallest_displacement_found)
                            {
                                smallest_displacement_found = displacement_D;
                                newProposedPos = ImVec2(currentPos.x, targetY_D);
                                move_found = true;
                            }
                        }

                        // Try moving Up (constant node above occRect)
                        float targetY_U = occRect.min.y - nodeSize.y - padding;
                        float displacement_U =
                          currentPos.y - targetY_U; // Must be positive for an upward move
                        if (displacement_U > 1e-4f)
                        {
                            if (displacement_U < smallest_displacement_found)
                            {
                                smallest_displacement_found = displacement_U;
                                newProposedPos = ImVec2(currentPos.x, targetY_U);
                                move_found = true;
                            }
                        }

                        if (move_found)
                        {
                            constantNode->screenPos() =
                              nodes::float2(newProposedPos.x, newProposedPos.y);
                        }
                        else
                        {
                            // Fallback: if no valid single-axis push found (e.g. node is contained,
                            // or stuck) Push it diagonally down-right from the overlapping rect's
                            // far corner as a last resort. This indicates a potentially tricky
                            // situation or very tight packing.
                            constantNode->screenPos().x = occRect.max.x + padding;
                            constantNode->screenPos().y = occRect.max.y + padding;
                        }

                        // Update nodeRect for the next check within the while loop or for final add
                        // to occupiedRects
                        nodeRect.min =
                          ImVec2(constantNode->screenPos().x, constantNode->screenPos().y);
                        nodeRect.max = ImVec2(constantNode->screenPos().x + nodeSize.x,
                                              constantNode->screenPos().y + nodeSize.y);

                        break; // Break from for-loop to re-check all occupiedRects with the new
                               // position
                    }
                }
                // If the for-loop completed without `hasOverlapIteration` being set to true, no
                // overlaps were found.
            }

            // After loop, node is placed. Add its final rect to occupiedRects.
            // Ensure nodeRect is based on the final position.
            nodeRect.min = ImVec2(constantNode->screenPos().x, constantNode->screenPos().y);
            nodeRect.max = ImVec2(constantNode->screenPos().x + nodeSize.x,
                                  constantNode->screenPos().y + nodeSize.y);
            occupiedRects.push_back(nodeRect);
        }

        model.markAsLayouted();
    } // ========== Generic Layout Algorithm ==========
    template <typename T>
    void NodeLayoutEngine::performLayeredLayout(std::vector<LayoutEntity<T>> & entities,
                                                const LayoutConfig & config,
                                                const std::vector<Rect> & occupiedRects)
    {
        if (entities.empty())
        {
            return;
        }

        // Step 1: Arrange entities in layers based on depth
        auto layers = arrangeInLayers(entities);

        // Step 2: Calculate layer X positions
        std::map<int, float> layerXPositions;
        float currentX = 0.0f;
        for (auto & [depth, layerEntities] : layers)
        {
            layerXPositions[depth] = currentX;
            float maxWidth = 0.0f;
            for (auto * entity : layerEntities)
            {
                maxWidth = std::max(maxWidth, entity->size.x);
            }
            currentX += maxWidth + config.layerSpacing;
        }

        // Step 3: Position entities in Y direction within each layer, avoiding occupied spaces
        for (auto & [depth, layerEntities] : layers)
        {
            float currentY = 0.0f;
            for (auto * entity : layerEntities)
            {
                entity->position.x = layerXPositions[depth];
                entity->position.y = currentY;

                // Check for overlap with occupied spaces and shift down if needed
                bool overlap = true;
                int maxTries = 1000; // avoid infinite loop
                while (overlap && maxTries-- > 0)
                {
                    overlap = false;
                    Rect entityRect(ImVec2(entity->position.x, entity->position.y),
                                    ImVec2(entity->position.x + entity->size.x,
                                           entity->position.y + entity->size.y));

                    for (const auto & occRect : occupiedRects)
                    {
                        if (entityRect.overlaps(occRect))
                        {
                            // Overlap detected, shift entity down
                            entity->position.y = occRect.max.y + config.nodeDistance;
                            currentY = entity->position.y;
                            overlap = true;
                            break;
                        }
                    }
                }
                currentY = entity->position.y + entity->size.y + config.nodeDistance;
            }
        }

        // Step 4: Optimize positions to minimize crossings
        optimizeLayerPositions(layers, config);
        // Step 5: Apply positions back to entities (handled by specific layout methods)
    }

    // ========== Constant Node Handling ==========

    bool NodeLayoutEngine::isConstantNode(nodes::NodeBase * node) const
    {
        auto const & nodeName = node->name();
        return nodeName == "ConstantScalar" || nodeName == "ConstantVector" ||
               nodeName == "ConstantMatrix";
    }

    ImVec2 NodeLayoutEngine::calculateConstantNodePosition(nodes::NodeBase * constantNode,
                                                           nodes::Model & model,
                                                           const LayoutConfig & config)
    {
        auto const graph = model.getGraph();
        auto const nodeId = constantNode->getId();

        // Find connected nodes
        std::vector<nodes::NodeBase *> connectedNodes;
        for (auto & [id, node] : model)
        {
            // Ensure the connected node itself is not a constant node being processed later,
            // or rely on the fact that screenPos for already laid out nodes is stable.
            // For this calculation, we use the current screenPos of connected nodes.
            if (id != nodeId && (graph.isDirectlyDependingOn(id, nodeId) ||
                                 graph.isDirectlyDependingOn(nodeId, id)))
            {
                // We only care about non-constant connected nodes for initial anchor,
                // or constants that have already been placed.
                // However, at this stage, other constants are not yet in `occupiedRects` with final
                // positions. So, we connect to any node type.
                connectedNodes.push_back(node.get());
            }
        }

        if (connectedNodes.empty())
        {
            // No connections, place at a default relative origin (will be adjusted by overlap)
            return ImVec2(0.0f, 0.0f);
        }

        auto constantNodeSize = ed::GetNodeSize(constantNode->getId());
        if (constantNodeSize.x <= 0.0f)
            constantNodeSize.x = 150.0f; // Default size from performAutoLayout
        if (constantNodeSize.y <= 0.0f)
            constantNodeSize.y = 80.0f; // Default size from performAutoLayout

        float sumConnectedCenterY = 0.0f;
        float overallMinX = std::numeric_limits<float>::max();

        for (auto * connected : connectedNodes)
        {
            auto & connPos = connected->screenPos();
            auto connSize = ed::GetNodeSize(connected->getId());
            // Use default/fallback sizes for connected nodes if GetNodeSize is not yet accurate
            // These defaults are from calculateEntitySize typically used for non-constant nodes.
            if (connSize.x <= 0.0f)
                connSize.x = 200.0f;
            if (connSize.y <= 0.0f)
                connSize.y = 100.0f;

            sumConnectedCenterY += connPos.y + connSize.y / 2.0f;
            overallMinX = std::min(overallMinX, connPos.x);
        }

        float avgConnectedCenterY = sumConnectedCenterY / static_cast<float>(connectedNodes.size());

        // Ideal position: to the left of the leftmost connected node,
        // vertically centered with the average center of connected nodes.
        float idealX = overallMinX - constantNodeSize.x - (config.nodeDistance / 2.0f); // Padding
        float idealY = avgConnectedCenterY - constantNodeSize.y / 2.0f;

        // Ensure idealX is not excessively negative if all connected nodes are at x=0
        // This might be relevant if the graph starts at x=0.
        // However, the overlap resolution should handle this.
        // For now, let's allow potentially negative local X, as it's relative.
        // idealX = std::max(0.0f, idealX); // Optional: prevent negative X relative to leftmost
        // connected.

        return ImVec2(idealX, idealY);
    }

    // ========== Group Analysis ==========

    std::vector<NodeLayoutEngine::GroupInfo>
    NodeLayoutEngine::analyzeGroups(nodes::Model & model,
                                    const std::unordered_map<nodes::NodeId, int> & depthMap)
    {
        std::unordered_map<std::string, GroupInfo> groupMap;

        // Collect nodes by group
        for (auto & [id, node] : model)
        {
            const std::string & tag = node->getTag();
            if (tag.empty() || isConstantNode(node.get()))
            {
                continue; // Skip ungrouped and constant nodes
            }

            auto depthIter = depthMap.find(id);
            int nodeDepth = (depthIter != depthMap.end()) ? depthIter->second : 0;

            if (groupMap.find(tag) == groupMap.end())
            {
                GroupInfo newGroup;
                newGroup.tag = tag;
                newGroup.nodes = {node.get()};
                newGroup.minDepth = nodeDepth;
                newGroup.maxDepth = nodeDepth;
                newGroup.position = ImVec2(0, 0);
                newGroup.size = ImVec2(0, 0);
                groupMap[tag] = std::move(newGroup);
            }
            else
            {
                auto & groupInfo = groupMap[tag];
                groupInfo.nodes.push_back(node.get());
                groupInfo.minDepth = std::min(groupInfo.minDepth, nodeDepth);
                groupInfo.maxDepth = std::max(groupInfo.maxDepth, nodeDepth);
            }
        }

        std::vector<GroupInfo> result;
        for (auto & [tag, info] : groupMap)
        {
            result.push_back(std::move(info));
        }

        return result;
    }

    // ========== Specific Layout Methods ==========

    void
    NodeLayoutEngine::layoutUngroupedNodes(const std::vector<nodes::NodeBase *> & ungroupedNodes,
                                           const std::unordered_map<nodes::NodeId, int> & depthMap,
                                           const LayoutConfig & config,
                                           const std::vector<Rect> & occupiedRects)
    {
        if (ungroupedNodes.empty())
        {
            return;
        }

        // Create entities for generic layout
        std::vector<NodeEntity> entities;
        entities.reserve(ungroupedNodes.size());

        for (auto * node : ungroupedNodes)
        {
            auto depthIter = depthMap.find(node->getId());
            int depth = (depthIter != depthMap.end()) ? depthIter->second : 0;

            entities.emplace_back(node, depth);
            auto & entity = entities.back();
            entity.size = calculateEntitySize(entity);
        }

        // Apply generic layered layout
        performLayeredLayout(entities, config, occupiedRects);

        // Apply results back to nodes
        for (size_t i = 0; i < entities.size(); ++i)
        {
            ungroupedNodes[i]->screenPos() =
              nodes::float2(entities[i].position.x, entities[i].position.y);
        }
    }

    void
    NodeLayoutEngine::layoutNodesInGroup(GroupInfo & groupInfo,
                                         const std::unordered_map<nodes::NodeId, int> & depthMap,
                                         const LayoutConfig & config,
                                         const std::vector<Rect> & occupiedRects)
    {
        if (groupInfo.nodes.empty())
        {
            return;
        }

        // Create entities for nodes in this group
        std::vector<NodeEntity> entities;
        entities.reserve(groupInfo.nodes.size());

        for (auto * node : groupInfo.nodes)
        {
            auto depthIter = depthMap.find(node->getId());
            int depth = (depthIter != depthMap.end()) ? depthIter->second : 0;

            entities.emplace_back(node, depth);
            auto & entity = entities.back();
            entity.size = calculateEntitySize(entity);
        }

        // Apply generic layered layout with tighter spacing for grouped nodes
        LayoutConfig groupConfig = config;
        groupConfig.nodeDistance *= 0.7f; // Tighter spacing within groups
        groupConfig.layerSpacing *= 0.8f;

        performLayeredLayout(entities, groupConfig, occupiedRects);

        // Apply results back to nodes
        for (size_t i = 0; i < entities.size(); ++i)
        {
            groupInfo.nodes[i]->screenPos() =
              nodes::float2(entities[i].position.x, entities[i].position.y);
        }

        // Update group bounds
        updateGroupBounds(groupInfo);
    }

    void NodeLayoutEngine::layoutGroups(std::vector<GroupInfo> & groups,
                                        const LayoutConfig & config)
    {
        if (groups.empty())
        {
            return;
        }

        // Sort groups by their minimum depth to maintain topological order
        std::sort(groups.begin(),
                  groups.end(),
                  [](const GroupInfo & a, const GroupInfo & b) { return a.minDepth < b.minDepth; });

        // Layout groups with sufficient spacing to avoid overlaps
        ImVec2 currentPos(0.0f, 0.0f);
        float maxGroupHeight = 0.0f;
        int currentDepth = -1;

        for (auto & group : groups)
        {
            // If we're at a new depth level, move to next "layer"
            if (group.minDepth != currentDepth)
            {
                if (currentDepth != -1)
                {
                    currentPos.x += config.layerSpacing;
                    currentPos.y = 0.0f; // Reset Y for new layer
                }
                currentDepth = group.minDepth;
            }
            else
            {
                // Same depth, stack vertically
                currentPos.y += maxGroupHeight + config.groupPadding;
            }

            // Apply group offset to all nodes in the group
            ImVec2 groupOffset = currentPos;
            for (auto * node : group.nodes)
            {
                auto & nodePos = node->screenPos();
                nodePos.x += groupOffset.x;
                nodePos.y += groupOffset.y;
            }

            // Update group position and size
            updateGroupBounds(group);
            maxGroupHeight = std::max(maxGroupHeight, group.size.y);
        }
    }

    void NodeLayoutEngine::resolveOverlaps(const std::vector<nodes::NodeBase *> & ungroupedNodes,
                                           const std::vector<nodes::NodeBase *> & constantNodes,
                                           std::vector<GroupInfo> & groups,
                                           const LayoutConfig & config)
    {
        constexpr float MIN_SPACING = 50.0f;
        const int MAX_ITERATIONS = config.maxOptimizationIterations;

        for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration)
        {
            bool hasOverlaps = false;

            // Check overlaps between ungrouped nodes and groups
            for (auto * node : ungroupedNodes)
            {
                auto nodePos = node->screenPos();
                auto nodeSize = ed::GetNodeSize(node->getId());
                if (nodeSize.x <= 0.0f)
                    nodeSize.x = 500.0f;
                if (nodeSize.y <= 0.0f)
                    nodeSize.y = 400.0f;

                for (auto & group : groups)
                {
                    // Check if node overlaps with group
                    if (nodePos.x < group.position.x + group.size.x + MIN_SPACING &&
                        nodePos.x + nodeSize.x + MIN_SPACING > group.position.x &&
                        nodePos.y < group.position.y + group.size.y + MIN_SPACING &&
                        nodePos.y + nodeSize.y + MIN_SPACING > group.position.y)
                    {
                        // Move node away from group (prefer moving left/down)
                        float deltaX = (group.position.x + group.size.x + MIN_SPACING) - nodePos.x;
                        float deltaY = (group.position.y + group.size.y + MIN_SPACING) - nodePos.y;

                        if (std::abs(deltaX) < std::abs(deltaY))
                        {
                            node->screenPos().x -= deltaX;
                        }
                        else
                        {
                            node->screenPos().y += deltaY;
                        }
                        hasOverlaps = true;
                    }
                }
            }

            // Check overlaps between constant nodes and groups
            for (auto * constantNode : constantNodes)
            {
                auto nodePos = constantNode->screenPos();
                auto nodeSize = ed::GetNodeSize(constantNode->getId());
                if (nodeSize.x <= 0.0f)
                    nodeSize.x = 500.0f;
                if (nodeSize.y <= 0.0f)
                    nodeSize.y = 400.0f;

                for (auto & group : groups)
                {
                    // Check if constant node overlaps with group
                    if (nodePos.x < group.position.x + group.size.x + MIN_SPACING &&
                        nodePos.x + nodeSize.x + MIN_SPACING > group.position.x &&
                        nodePos.y < group.position.y + group.size.y + MIN_SPACING &&
                        nodePos.y + nodeSize.y + MIN_SPACING > group.position.y)
                    {
                        // Move constant node away from group (prefer moving left since constants
                        // are typically to the left)
                        float deltaX = group.position.x - (nodePos.x + nodeSize.x + MIN_SPACING);
                        float deltaY = (group.position.y + group.size.y + MIN_SPACING) - nodePos.y;

                        if (std::abs(deltaX) < std::abs(deltaY) && deltaX < 0)
                        {
                            constantNode->screenPos().x -= deltaX;
                        }
                        else
                        {
                            constantNode->screenPos().y += deltaY;
                        }
                        hasOverlaps = true;
                    }
                }
            }

            // Check overlaps between constant nodes and ungrouped nodes
            for (auto * constantNode : constantNodes)
            {
                auto constantPos = constantNode->screenPos();
                auto constantSize = ed::GetNodeSize(constantNode->getId());
                if (constantSize.x <= 0.0f)
                    constantSize.x = 500.0f;
                if (constantSize.y <= 0.0f)
                    constantSize.y = 400.0f;

                for (auto * regularNode : ungroupedNodes)
                {
                    auto nodePos = regularNode->screenPos();
                    auto nodeSize = ed::GetNodeSize(regularNode->getId());
                    if (nodeSize.x <= 0.0f)
                        nodeSize.x = 500.0f;
                    if (nodeSize.y <= 0.0f)
                        nodeSize.y = 400.0f;

                    // Check if constant node overlaps with regular node
                    if (constantPos.x < nodePos.x + nodeSize.x + MIN_SPACING &&
                        constantPos.x + constantSize.x + MIN_SPACING > nodePos.x &&
                        constantPos.y < nodePos.y + nodeSize.y + MIN_SPACING &&
                        constantPos.y + constantSize.y + MIN_SPACING > nodePos.y)
                    {
                        // Move constant node away (prefer moving left)
                        float deltaX = nodePos.x - (constantPos.x + constantSize.x + MIN_SPACING);
                        float deltaY = (nodePos.y + nodeSize.y + MIN_SPACING) - constantPos.y;

                        if (std::abs(deltaX) < std::abs(deltaY) && deltaX < 0)
                        {
                            constantNode->screenPos().x -= deltaX;
                        }
                        else
                        {
                            constantNode->screenPos().y += deltaY;
                        }
                        hasOverlaps = true;
                    }
                }
            }

            // Check overlaps between constant nodes themselves
            for (size_t i = 0; i < constantNodes.size(); ++i)
            {
                auto * node1 = constantNodes[i];
                auto pos1 = node1->screenPos();
                auto size1 = ed::GetNodeSize(node1->getId());
                if (size1.x <= 0.0f)
                    size1.x = 500.0f;
                if (size1.y <= 0.0f)
                    size1.y = 400.0f;

                for (size_t j = i + 1; j < constantNodes.size(); ++j)
                {
                    auto * node2 = constantNodes[j];
                    auto pos2 = node2->screenPos();
                    auto size2 = ed::GetNodeSize(node2->getId());
                    if (size2.x <= 0.0f)
                        size2.x = 500.0f;
                    if (size2.y <= 0.0f)
                        size2.y = 400.0f;

                    // Check if constant nodes overlap
                    if (pos1.x < pos2.x + size2.x + MIN_SPACING &&
                        pos1.x + size1.x + MIN_SPACING > pos2.x &&
                        pos1.y < pos2.y + size2.y + MIN_SPACING &&
                        pos1.y + size1.y + MIN_SPACING > pos2.y)
                    {
                        // Move nodes apart vertically (stack them)
                        float overlapY = (pos1.y + size1.y + MIN_SPACING) - pos2.y;
                        if (overlapY > 0)
                        {
                            node2->screenPos().y += overlapY;
                        }
                        else
                        {
                            node1->screenPos().y -= overlapY;
                        }
                        hasOverlaps = true;
                    }
                }
            }

            if (!hasOverlaps)
            {
                break; // No more overlaps, we're done
            }
        }
    }

    // ========== Helper Methods ==========

    std::unordered_map<nodes::NodeId, int>
    NodeLayoutEngine::determineDepth(const nodes::graph::IDirectedGraph & graph,
                                     nodes::NodeId beginId)
    {
        return nodes::graph::determineDepth(graph, beginId);
    }

    template <typename T>
    std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T> *>>
    NodeLayoutEngine::arrangeInLayers(std::vector<LayoutEntity<T>> & entities)
    {
        std::map<int, std::vector<LayoutEntity<T> *>> layers;

        for (auto & entity : entities)
        {
            layers[entity.depth].push_back(&entity);
        }

        // Sort within each layer (for consistent ordering)
        for (auto & [depth, layerEntities] : layers)
        {
            std::sort(layerEntities.begin(),
                      layerEntities.end(),
                      [](const LayoutEntity<T> * a, const LayoutEntity<T> * b)
                      {
                          return a->item < b->item; // Pointer comparison for consistency
                      });
        }

        return layers;
    }

    template <typename T>
    void
    NodeLayoutEngine::optimizeLayerPositions(std::map<int, std::vector<LayoutEntity<T> *>> & layers,
                                             const LayoutConfig & config)
    {
        // Simple optimization: distribute entities evenly within each layer
        for (auto & [depth, layerEntities] : layers)
        {
            if (layerEntities.size() <= 1)
            {
                continue;
            }

            // Sort by current Y position
            std::sort(layerEntities.begin(),
                      layerEntities.end(),
                      [](const LayoutEntity<T> * a, const LayoutEntity<T> * b)
                      { return a->position.y < b->position.y; });

            // Redistribute with consistent spacing
            float currentY = 0.0f;
            for (auto * entity : layerEntities)
            {
                entity->position.y = currentY;
                currentY += entity->size.y + config.nodeDistance;
            }
        }
    }

    ImVec2 NodeLayoutEngine::calculateEntitySize(NodeEntity & entity)
    {
        auto nodeSize = ed::GetNodeSize(entity.item->getId());
        if (nodeSize.x <= 0.0f)
            nodeSize.x = 500.0f; // Default width
        if (nodeSize.y <= 0.0f)
            nodeSize.y = 400.0f; // Default height
        return nodeSize;
    }

    ImVec2 NodeLayoutEngine::calculateGroupSize(const GroupInfo & groupInfo)
    {
        if (groupInfo.nodes.empty())
        {
            return ImVec2(0, 0);
        }

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (auto * node : groupInfo.nodes)
        {
            auto pos = node->screenPos();
            auto size = ed::GetNodeSize(node->getId());
            if (size.x <= 0.0f)
                size.x = 500.0f;
            if (size.y <= 0.0f)
                size.y = 400.0f;

            minX = std::min(minX, pos.x);
            minY = std::min(minY, pos.y);
            maxX = std::max(maxX, pos.x + size.x);
            maxY = std::max(maxY, pos.y + size.y);
        }

        return ImVec2(maxX - minX, maxY - minY);
    }

    void NodeLayoutEngine::updateGroupBounds(GroupInfo & groupInfo)
    {
        if (groupInfo.nodes.empty())
        {
            return;
        }

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (auto * node : groupInfo.nodes)
        {
            auto pos = node->screenPos();
            auto size = ed::GetNodeSize(node->getId());
            if (size.x <= 0.0f)
                size.x = 500.0f;
            if (size.y <= 0.0f)
                size.y = 400.0f;

            minX = std::min(minX, pos.x);
            minY = std::min(minY, pos.y);
            maxX = std::max(maxX, pos.x + size.x);
            maxY = std::max(maxY, pos.y + size.y);
        }

        groupInfo.position = ImVec2(minX, minY);
        groupInfo.size = ImVec2(maxX - minX, maxY - minY);
    } // We'll let the compiler implicitly instantiate the templates as needed

} // namespace gladius::ui
