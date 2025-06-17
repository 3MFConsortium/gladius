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
            layoutNodesInGroup(
              group, depthMap, config, occupiedRects);
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
            layoutUngroupedNodes(
              ungroupedNodes, depthMap, config, occupiedRects);

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
