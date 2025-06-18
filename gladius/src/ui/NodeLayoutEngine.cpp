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

        // Step 1: Separate nodes into ungrouped and identify those in groups
        // All nodes (including constants) are treated equally in the layered layout
        std::vector<nodes::NodeBase *> ungroupedNodesPreFilter;

        for (auto & [id, node] : model)
        {
            if (node->getTag().empty())
            {
                ungroupedNodesPreFilter.push_back(node.get());
            }
        }

        // Step 2: Analyze groups using the depth map
        auto groups = analyzeGroups(model, depthMap);

        // Filter ungroupedNodes, ensuring they are not part of any analyzed group
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

        // Step 3: Layout each group separately and track occupied spaces
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
            layoutNodesInGroup(group, depthMap, config, occupiedRects);
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

        // Step 4: Place ungrouped nodes after all groups, considering occupied spaces
        if (!ungroupedNodes.empty()) // Check if there are any truly ungrouped nodes
        {                            // Layout ungrouped nodes (local coordinates)
            layoutUngroupedNodes(ungroupedNodes, depthMap, config, occupiedRects);

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

        // Step 4: Optimize positions to minimize crossings using the simple approach
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
            if (tag.empty())
            {
                continue; // Skip ungrouped nodes
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
        std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T> *>> layers;

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

        // Establish connections between entities based on the graph structure
        if constexpr (std::is_same_v<T, nodes::NodeBase>)
        {
            // Create lookup map for faster entity lookup by node ID
            std::unordered_map<nodes::NodeId, LayoutEntity<T> *> entityByNodeId;
            for (auto & entity : entities)
            {
                entityByNodeId[entity.item->getId()] = &entity;
            }

            // Clear existing connections
            for (auto & entity : entities)
            {
                entity.dependencies.clear();
                entity.dependents.clear();
            }

            // For each entity, find its dependencies and dependents
            for (auto & entity : entities)
            {
                nodes::NodeId nodeId = entity.item->getId();

                // For each node, find its dependencies in the model
                // This would require access to the graph or model to determine actual connections
                // For now, we'll use depth as a heuristic - nodes in lower layers are dependencies
                for (auto & otherEntity : entities)
                {
                    if (otherEntity.item == entity.item)
                        continue;

                    nodes::NodeId otherId = otherEntity.item->getId();

                    // Use the graph structure to determine actual dependencies
                    // We need access to the model/graph to check direct connections
                    // For now, use depth-based heuristic with actual connection verification later
                    bool hasConnection = false;

                    // Check if there's a direct dependency relationship
                    // This would need access to the graph structure from the model
                    if (std::abs(entity.depth - otherEntity.depth) == 1)
                    {
                        if (otherEntity.depth < entity.depth)
                        {
                            // Other entity is in a previous layer, could be a dependency
                            entity.dependencies.push_back(&otherEntity);
                            otherEntity.dependents.push_back(&entity);
                            hasConnection = true;
                        }
                        else
                        {
                            // Other entity is in a later layer, this entity could be its dependency
                            otherEntity.dependencies.push_back(&entity);
                            entity.dependents.push_back(&otherEntity);
                            hasConnection = true;
                        }
                    }
                }
            }
        }

        return layers;
    }

    template <typename T>
    void NodeLayoutEngine::optimizeLayerPositions(
      std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T> *>> & layers,
      const LayoutConfig & config)
    {
        if (layers.empty())
            return;

        // Process layers from right to left (highest depth to lowest)
        std::vector<int> layerDepths;
        for (auto & [depth, _] : layers)
        {
            layerDepths.push_back(depth);
        }
        std::sort(layerDepths.rbegin(), layerDepths.rend()); // Reverse sort for right-to-left

        for (int depth : layerDepths)
        {
            auto & layerEntities = layers[depth];
            if (layerEntities.size() <= 1)
            {
                // Single node - just center it
                if (layerEntities.size() == 1)
                {
                    layerEntities[0]->position.y = 0.0f;
                }
                continue;
            }

            optimizeLayerByConnectionOrder(layerEntities, layers, depth, config);
        }
    }

    template <typename T>
    void NodeLayoutEngine::optimizeLayerByConnectionOrder(
      std::vector<NodeLayoutEngine::LayoutEntity<T> *> & layerEntities,
      const std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T> *>> & allLayers,
      int currentDepth,
      const LayoutConfig & config)
    {
        if (layerEntities.empty())
            return;

        // Calculate average Y position of connected nodes for each entity in this layer
        std::vector<std::pair<NodeLayoutEngine::LayoutEntity<T> *, float>> entityWithAvgY;

        for (auto * entity : layerEntities)
        {
            float avgConnectedY =
              calculateAverageConnectedY(entity, allLayers, currentDepth, config);
            entityWithAvgY.emplace_back(entity, avgConnectedY);
        }

        // Sort entities by their average connected Y position
        std::sort(entityWithAvgY.begin(),
                  entityWithAvgY.end(),
                  [](const auto & a, const auto & b) { return a.second < b.second; });

        // Stack nodes vertically with proper spacing
        float currentY = 0.0f;
        for (auto & [entity, avgY] : entityWithAvgY)
        {
            entity->position.y = currentY;
            currentY += entity->size.y + config.nodeDistance;
        }

        // Center the layer vertically
        float totalHeight = currentY - config.nodeDistance; // Remove last spacing
        float centerOffset = -totalHeight / 2.0f;

        for (auto & [entity, avgY] : entityWithAvgY)
        {
            entity->position.y += centerOffset;
        }
    }

    template <typename T>
    float NodeLayoutEngine::calculateAverageConnectedY(
      NodeLayoutEngine::LayoutEntity<T> * entity,
      const std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T> *>> & allLayers,
      int currentDepth,
      const LayoutConfig & config)
    {
        if constexpr (std::is_same_v<T, nodes::NodeBase>)
        {
            std::vector<float> connectedYPositions;

            // Use the dependencies and dependents we already established
            for (auto * dep : entity->dependencies)
            {
                float connectedCenterY = dep->position.y + dep->size.y / 2.0f;
                connectedYPositions.push_back(connectedCenterY);
            }

            for (auto * dep : entity->dependents)
            {
                float connectedCenterY = dep->position.y + dep->size.y / 2.0f;
                connectedYPositions.push_back(connectedCenterY);
            }

            if (!connectedYPositions.empty())
            {
                // Return average Y position of all connected nodes
                float sum = 0.0f;
                for (float y : connectedYPositions)
                {
                    sum += y;
                }
                return sum / static_cast<float>(connectedYPositions.size());
            }
        }

        // If no connections found, return current center Y
        return entity->position.y + entity->size.y / 2.0f;
    }

    template <typename T>
    bool NodeLayoutEngine::areNodesConnected(T * node1, T * node2)
    {
        if constexpr (std::is_same_v<T, nodes::NodeBase>)
        {
            nodes::NodeId id1 = node1->getId();
            nodes::NodeId id2 = node2->getId();

            // For now, use a simple approach based on the dependency relationships
            // we established earlier in arrangeInLayers
            // In a real implementation, we would need access to the model/graph
            // to check actual port connections

            // This is a placeholder - we'll need to pass the model/graph
            // to this method to check actual connections
            return false; // Will be implemented properly when we have graph access
        }

        return false;
    }

    template <typename T>
    void NodeLayoutEngine::optimizeSingleLayer(
      std::vector<NodeLayoutEngine::LayoutEntity<T> *> & layerEntities,
      const std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T> *>> & allLayers,
      int currentDepth,
      const LayoutConfig & config)
    {
        const int MAX_ITERATIONS = 50;
        const float CONVERGENCE_THRESHOLD = 1.0f;

        // Group entities by their group membership (for nodes)
        std::vector<std::vector<NodeLayoutEngine::LayoutEntity<T> *>> entityGroups;
        std::vector<NodeLayoutEngine::LayoutEntity<T> *> ungroupedEntities;

        if constexpr (std::is_same_v<T, nodes::NodeBase>)
        {
            groupEntitiesByTag(
              reinterpret_cast<const std::vector<LayoutEntity<nodes::NodeBase> *> &>(layerEntities),
              reinterpret_cast<std::vector<std::vector<LayoutEntity<nodes::NodeBase> *>> &>(
                entityGroups),
              reinterpret_cast<std::vector<LayoutEntity<nodes::NodeBase> *> &>(ungroupedEntities));
        }
        else
        {
            // For non-node entities, treat each as individual
            for (auto * entity : layerEntities)
            {
                ungroupedEntities.push_back(entity);
            }
        }

        // Combine groups and individual entities for optimization
        std::vector<OptimizationUnit<T>> optimizationUnits;

        // Add groups as units
        for (auto & group : entityGroups)
        {
            optimizationUnits.emplace_back(group);
        }

        // Add individual entities as units
        for (auto * entity : ungroupedEntities)
        {
            optimizationUnits.emplace_back(
              std::vector<NodeLayoutEngine::LayoutEntity<T> *>{entity});
        }

        // Iterative optimization
        for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration)
        {
            float totalMovement = 0.0f;

            for (auto & unit : optimizationUnits)
            {
                float oldY = calculateUnitCenterY(unit);
                float optimalY = calculateOptimalYPosition(unit, allLayers, currentDepth, config);

                // Move the unit to optimal position
                moveOptimizationUnit(unit, optimalY);

                float newY = calculateUnitCenterY(unit);
                totalMovement += std::abs(newY - oldY);
            }

            // Resolve overlaps while preserving group clustering
            resolveLayerOverlaps(optimizationUnits, config);

            // Check for convergence
            if (totalMovement < CONVERGENCE_THRESHOLD)
            {
                break;
            }
        }
    }

    void NodeLayoutEngine::groupEntitiesByTag(
      const std::vector<LayoutEntity<nodes::NodeBase> *> & entities,
      std::vector<std::vector<LayoutEntity<nodes::NodeBase> *>> & groups,
      std::vector<LayoutEntity<nodes::NodeBase> *> & ungrouped)
    {
        std::unordered_map<std::string, std::vector<LayoutEntity<nodes::NodeBase> *>> tagToEntities;

        for (auto * entity : entities)
        {
            const std::string & tag = entity->item->getTag();
            if (tag.empty())
            {
                ungrouped.push_back(entity);
            }
            else
            {
                tagToEntities[tag].push_back(entity);
            }
        }

        // Convert map to vector of groups
        for (auto & [tag, groupEntities] : tagToEntities)
        {
            groups.push_back(std::move(groupEntities));
        }
    }

    template <typename T>
    float NodeLayoutEngine::calculateUnitCenterY(const OptimizationUnit<T> & unit)
    {
        if (unit.entities.empty())
            return 0.0f;

        float sumY = 0.0f;
        for (auto * entity : unit.entities)
        {
            sumY += entity->position.y + entity->size.y / 2.0f;
        }
        return sumY / static_cast<float>(unit.entities.size());
    }

    template <typename T>
    float NodeLayoutEngine::calculateOptimalYPosition(
      const OptimizationUnit<T> & unit,
      const std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T> *>> & allLayers,
      int currentDepth,
      const LayoutConfig & config)
    {
        float totalWeight = 0.0f;
        float weightedSum = 0.0f;

        // Calculate weighted average of connected nodes' positions
        for (auto * entity : unit.entities)
        {
            if constexpr (std::is_same_v<T, nodes::NodeBase>)
            {
                // Track both direct connections and group connections
                // Direct connections have higher weight
                auto directConnections = getNodeConnections(entity->item, allLayers, currentDepth);

                // Process direct connections
                for (auto & [connectedEntity, weight] : directConnections)
                {
                    // Check if the connected entity is part of a group
                    std::string connectedTag;
                    if (connectedEntity->item)
                    {
                        connectedTag = connectedEntity->item->getTag();
                    }

                    // For connections to entities in the same group, increase weight
                    // to keep groups more tightly connected
                    std::string thisTag;
                    if (entity->item)
                    {
                        thisTag = entity->item->getTag();
                    }

                    if (!thisTag.empty() && thisTag == connectedTag)
                    {
                        // Same group connections are weighted higher to keep groups together
                        weight *= 2.0f;
                    }

                    float connectedCenterY =
                      connectedEntity->position.y + connectedEntity->size.y / 2.0f;
                    weightedSum += connectedCenterY * weight;
                    totalWeight += weight;
                }

                // For grouped nodes, also consider connections to other nodes in the same group
                // that might not be directly connected
                std::string entityTag;
                if (entity->item)
                {
                    entityTag = entity->item->getTag();
                }

                if (!entityTag.empty())
                {
                    for (auto & [layerDepth, layerEntities] : allLayers)
                    {
                        if (std::abs(layerDepth - currentDepth) > 1)
                            continue; // Only adjacent layers

                        for (auto * otherEntity : layerEntities)
                        {
                            // Skip entities already processed via direct connections
                            bool alreadyProcessed = false;
                            for (auto & [processedEntity, w] : directConnections)
                            {
                                if (processedEntity == otherEntity)
                                {
                                    alreadyProcessed = true;
                                    break;
                                }
                            }
                            if (alreadyProcessed)
                                continue;

                            // Check if entity is in the same group
                            std::string otherTag;
                            if (otherEntity->item)
                            {
                                otherTag = otherEntity->item->getTag();
                            }

                            if (entityTag == otherTag && !otherTag.empty())
                            {
                                // Add a weaker connection to nodes in the same group
                                float groupWeight = 0.5f;
                                float otherCenterY =
                                  otherEntity->position.y + otherEntity->size.y / 2.0f;
                                weightedSum += otherCenterY * groupWeight;
                                totalWeight += groupWeight;
                            }
                        }
                    }
                }
            }
        }

        if (totalWeight > 0.0f)
        {
            float optimalCenterY = weightedSum / totalWeight;
            // Convert center Y to top Y for the unit
            float unitHeight = unit.getHeight();

            return optimalCenterY - unitHeight / 2.0f;
        }

        return calculateUnitCenterY(unit) - unit.getHeight() / 2.0f;
    }

    template <typename T>
    std::vector<std::pair<NodeLayoutEngine::LayoutEntity<T> *, float>>
    NodeLayoutEngine::getNodeConnections(
      T * node,
      const std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T> *>> & allLayers,
      int currentDepth)
    {
        std::vector<std::pair<NodeLayoutEngine::LayoutEntity<T> *, float>> connections;

        if constexpr (std::is_same_v<T, nodes::NodeBase>)
        {
            nodes::NodeId nodeId = node->getId();

            // Look in adjacent layers for connected nodes
            for (int deltaDepth : {-1, 1})
            {
                int targetDepth = currentDepth + deltaDepth;
                auto layerIt = allLayers.find(targetDepth);
                if (layerIt == allLayers.end())
                    continue;

                for (auto * candidateEntity : layerIt->second)
                {
                    nodes::NodeId candidateId = candidateEntity->item->getId();
                    bool isConnected = false;
                    float weight = 1.0f;

                    // Check if this node depends on the candidate (incoming edge)
                    for (auto * dep : candidateEntity->dependents)
                    {
                        if (dep->item->getId() == nodeId)
                        {
                            isConnected = true;
                            // Give slightly higher weight to direct connections
                            weight = 1.2f;
                            break;
                        }
                    }

                    // Check if candidate depends on this node (outgoing edge)
                    if (!isConnected)
                    {
                        for (auto * dep : candidateEntity->dependencies)
                        {
                            if (dep->item->getId() == nodeId)
                            {
                                isConnected = true;
                                // Give slightly higher weight to direct connections
                                weight = 1.2f;
                                break;
                            }
                        }
                    }

                    if (isConnected)
                    {
                        connections.emplace_back(candidateEntity, weight);
                    }
                }
            }
        }

        return connections;
    }

    template <typename T>
    void NodeLayoutEngine::moveOptimizationUnit(OptimizationUnit<T> & unit, float targetTopY)
    {
        if (unit.entities.empty())
            return;

        float currentTopY = unit.minY;
        float deltaY = targetTopY - currentTopY;

        // Move all entities in the unit by the same delta
        for (auto * entity : unit.entities)
        {
            entity->position.y += deltaY;
        }

        unit.updateBounds();
    }

    template <typename T>
    void NodeLayoutEngine::resolveLayerOverlaps(std::vector<OptimizationUnit<T>> & units,
                                                const LayoutConfig & config)
    {
        if (units.size() <= 1)
            return;

        // Sort units by their center Y position
        std::sort(units.begin(),
                  units.end(),
                  [](const OptimizationUnit<T> & a, const OptimizationUnit<T> & b)
                  { return a.getCenterY() < b.getCenterY(); });

        // Resolve overlaps by pushing units apart
        for (size_t i = 1; i < units.size(); ++i)
        {
            auto & prevUnit = units[i - 1];
            auto & currentUnit = units[i];

            float minRequiredTop = prevUnit.maxY + config.nodeDistance;
            if (currentUnit.minY < minRequiredTop)
            {
                float pushDistance = minRequiredTop - currentUnit.minY;
                moveOptimizationUnit(currentUnit, currentUnit.minY + pushDistance);
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

    // ========== OptimizationUnit Template Implementations ==========

    template <typename T>
    NodeLayoutEngine::OptimizationUnit<T>::OptimizationUnit(
      std::vector<NodeLayoutEngine::LayoutEntity<T> *> entities_)
        : entities(std::move(entities_))
    {
        updateBounds();
    }

    template <typename T>
    void NodeLayoutEngine::OptimizationUnit<T>::updateBounds()
    {
        if (entities.empty())
        {
            minY = 0.0f;
            maxY = 0.0f;
            return;
        }

        minY = std::numeric_limits<float>::max();
        maxY = std::numeric_limits<float>::lowest();

        for (auto * entity : entities)
        {
            minY = std::min(minY, entity->position.y);
            maxY = std::max(maxY, entity->position.y + entity->size.y);
        }
    }

    template <typename T>
    float NodeLayoutEngine::OptimizationUnit<T>::getHeight() const
    {
        return maxY - minY;
    }

    template <typename T>
    float NodeLayoutEngine::OptimizationUnit<T>::getCenterY() const
    {
        return (minY + maxY) / 2.0f;
    }

} // namespace gladius::ui
