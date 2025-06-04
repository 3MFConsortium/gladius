#include "NodeLayoutEngine.h"
#include "nodes/graph/GraphAlgorithms.h"
#include "imgui.h"
#include "imgui-node-editor/imgui_node_editor.h"
#include <algorithm>
#include <limits>
#include <unordered_set>

namespace ed = ax::NodeEditor;

namespace gladius::ui
{
    void NodeLayoutEngine::performAutoLayout(nodes::Model& model, const LayoutConfig& config)
    {
        auto const graph = model.getGraph();

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

        // Step 1: Analyze groups and separate nodes
        auto groups = analyzeGroups(model, depthMap);
        std::vector<nodes::NodeBase*> ungroupedNodes;
        std::vector<nodes::NodeBase*> constantNodes;

        for (auto& [id, node] : model)
        {
            if (isConstantNode(node.get()))
            {
                constantNodes.push_back(node.get());
            }
            else if (node->getTag().empty())
            {
                ungroupedNodes.push_back(node.get());
            }
            // Grouped nodes are already in groups
        }

        // Step 2: Layout ungrouped nodes using generic algorithm
        layoutUngroupedNodes(ungroupedNodes, depthMap, config);

        // Step 3: Layout nodes within each group using generic algorithm
        for (auto& group : groups)
        {
            layoutNodesInGroup(group, depthMap, config);
        }

        // Step 4: Layout groups themselves to avoid overlaps
        layoutGroups(groups, config);

        // Step 5: Position constant nodes close to their connected nodes
        for (auto* constantNode : constantNodes)
        {
            auto optimalPos = calculateConstantNodePosition(constantNode, model, config);
            constantNode->screenPos() = nodes::float2(optimalPos.x, optimalPos.y);
        }

        // Step 6: Final overlap resolution
        resolveOverlaps(ungroupedNodes, constantNodes, groups, config);

        model.markAsLayouted();
    }

    // ========== Generic Layout Algorithm ==========

    template<typename T>
    void NodeLayoutEngine::performLayeredLayout(std::vector<LayoutEntity<T>>& entities, const LayoutConfig& config)
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
        
        for (auto& [depth, layerEntities] : layers)
        {
            layerXPositions[depth] = currentX;
            
            // Find maximum width in this layer
            float maxWidth = 0.0f;
            for (auto* entity : layerEntities)
            {
                maxWidth = std::max(maxWidth, entity->size.x);
            }
            
            currentX += maxWidth + config.layerSpacing;
        }

        // Step 3: Position entities in Y direction within each layer
        for (auto& [depth, layerEntities] : layers)
        {
            float currentY = 0.0f;
            
            for (auto* entity : layerEntities)
            {
                entity->position.x = layerXPositions[depth];
                entity->position.y = currentY;
                currentY += entity->size.y + config.nodeDistance;
            }
        }

        // Step 4: Optimize positions to minimize crossings
        optimizeLayerPositions(layers, config);

        // Step 5: Apply positions back to entities
        for (auto& entity : entities)
        {
            // This will be handled by the specific layout methods
        }
    }

    // ========== Constant Node Handling ==========

    bool NodeLayoutEngine::isConstantNode(nodes::NodeBase* node) const
    {
        auto const& nodeName = node->name();
        return nodeName == "ConstantScalar" || 
               nodeName == "ConstantVector" || 
               nodeName == "ConstantMatrix";
    }

    ImVec2 NodeLayoutEngine::calculateConstantNodePosition(nodes::NodeBase* constantNode, 
                                                          nodes::Model& model, 
                                                          const LayoutConfig& config)
    {
        auto const graph = model.getGraph();
        auto const nodeId = constantNode->getId();
        
        // Find connected nodes
        std::vector<nodes::NodeBase*> connectedNodes;
        for (auto& [id, node] : model)
        {
            if (id != nodeId && (graph.isDirectlyDependingOn(id, nodeId) || graph.isDirectlyDependingOn(nodeId, id)))
            {
                connectedNodes.push_back(node.get());
            }
        }
        
        if (connectedNodes.empty())
        {
            // No connections, place at origin
            return ImVec2(0.0f, 0.0f);
        }
        
        // Calculate average position of connected nodes
        ImVec2 avgPosition(0.0f, 0.0f);
        float minX = std::numeric_limits<float>::max();
        
        for (auto* connected : connectedNodes)
        {
            auto& pos = connected->screenPos();
            avgPosition.x += pos.x;
            avgPosition.y += pos.y;
            minX = std::min(minX, pos.x);
        }
        
        avgPosition.x /= static_cast<float>(connectedNodes.size());
        avgPosition.y /= static_cast<float>(connectedNodes.size());
        
        // Position constant node to the left of the leftmost connected node
        // but not further left than necessary
        float constantX = minX - config.constantNodeOffset;
        constantX = std::max(constantX, 0.0f); // Don't go too far left
        
        return ImVec2(constantX, avgPosition.y);
    }

    // ========== Group Analysis ==========

    std::vector<NodeLayoutEngine::GroupInfo> NodeLayoutEngine::analyzeGroups(nodes::Model& model, 
                                                                             const std::unordered_map<nodes::NodeId, int>& depthMap)
    {
        std::unordered_map<std::string, GroupInfo> groupMap;
        
        // Collect nodes by group
        for (auto& [id, node] : model)
        {
            const std::string& tag = node->getTag();
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
                auto& groupInfo = groupMap[tag];
                groupInfo.nodes.push_back(node.get());
                groupInfo.minDepth = std::min(groupInfo.minDepth, nodeDepth);
                groupInfo.maxDepth = std::max(groupInfo.maxDepth, nodeDepth);
            }
        }
        
        std::vector<GroupInfo> result;
        for (auto& [tag, info] : groupMap)
        {
            result.push_back(std::move(info));
        }
        
        return result;
    }

    // ========== Specific Layout Methods ==========

    void NodeLayoutEngine::layoutUngroupedNodes(const std::vector<nodes::NodeBase*>& ungroupedNodes,
                                               const std::unordered_map<nodes::NodeId, int>& depthMap,
                                               const LayoutConfig& config)
    {
        if (ungroupedNodes.empty())
        {
            return;
        }

        // Create entities for generic layout
        std::vector<NodeEntity> entities;
        entities.reserve(ungroupedNodes.size());
        
        for (auto* node : ungroupedNodes)
        {
            auto depthIter = depthMap.find(node->getId());
            int depth = (depthIter != depthMap.end()) ? depthIter->second : 0;
            
            entities.emplace_back(node, depth);
            auto& entity = entities.back();
            entity.size = calculateEntitySize(entity);
        }

        // Apply generic layered layout
        performLayeredLayout(entities, config);

        // Apply results back to nodes
        for (size_t i = 0; i < entities.size(); ++i)
        {
            ungroupedNodes[i]->screenPos() = nodes::float2(entities[i].position.x, entities[i].position.y);
        }
    }

    void NodeLayoutEngine::layoutNodesInGroup(GroupInfo& groupInfo,
                                             const std::unordered_map<nodes::NodeId, int>& depthMap,
                                             const LayoutConfig& config)
    {
        if (groupInfo.nodes.empty())
        {
            return;
        }

        // Create entities for nodes in this group
        std::vector<NodeEntity> entities;
        entities.reserve(groupInfo.nodes.size());
        
        for (auto* node : groupInfo.nodes)
        {
            auto depthIter = depthMap.find(node->getId());
            int depth = (depthIter != depthMap.end()) ? depthIter->second : 0;
            
            entities.emplace_back(node, depth);
            auto& entity = entities.back();
            entity.size = calculateEntitySize(entity);
        }

        // Apply generic layered layout with tighter spacing for grouped nodes
        LayoutConfig groupConfig = config;
        groupConfig.nodeDistance *= 0.7f; // Tighter spacing within groups
        groupConfig.layerSpacing *= 0.8f;

        performLayeredLayout(entities, groupConfig);

        // Apply results back to nodes
        for (size_t i = 0; i < entities.size(); ++i)
        {
            groupInfo.nodes[i]->screenPos() = nodes::float2(entities[i].position.x, entities[i].position.y);
        }

        // Update group bounds
        updateGroupBounds(groupInfo);
    }

    void NodeLayoutEngine::layoutGroups(std::vector<GroupInfo>& groups, const LayoutConfig& config)
    {
        if (groups.empty())
        {
            return;
        }

        // Sort groups by their minimum depth to maintain topological order
        std::sort(groups.begin(), groups.end(),
                 [](const GroupInfo& a, const GroupInfo& b) {
                     return a.minDepth < b.minDepth;
                 });

        // Layout groups with sufficient spacing to avoid overlaps
        ImVec2 currentPos(0.0f, 0.0f);
        float maxGroupHeight = 0.0f;
        int currentDepth = -1;

        for (auto& group : groups)
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
            for (auto* node : group.nodes)
            {
                auto& nodePos = node->screenPos();
                nodePos.x += groupOffset.x;
                nodePos.y += groupOffset.y;
            }

            // Update group position and size
            updateGroupBounds(group);
            maxGroupHeight = std::max(maxGroupHeight, group.size.y);
        }
    }

    void NodeLayoutEngine::resolveOverlaps(const std::vector<nodes::NodeBase*>& ungroupedNodes,
                                         const std::vector<nodes::NodeBase*>& constantNodes,
                                         std::vector<GroupInfo>& groups,
                                         const LayoutConfig& config)
    {
        constexpr float MIN_SPACING = 20.0f;
        const int MAX_ITERATIONS = config.maxOptimizationIterations;

        for (int iteration = 0; iteration < MAX_ITERATIONS; ++iteration)
        {
            bool hasOverlaps = false;

            // Check overlaps between ungrouped nodes and groups
            for (auto* node : ungroupedNodes)
            {
                auto nodePos = node->screenPos();
                auto nodeSize = ed::GetNodeSize(node->getId());
                if (nodeSize.x <= 0.0f) nodeSize.x = 200.0f;
                if (nodeSize.y <= 0.0f) nodeSize.y = 100.0f;

                for (auto& group : groups)
                {
                    // Check if node overlaps with group
                    if (nodePos.x < group.position.x + group.size.x + MIN_SPACING &&
                        nodePos.x + nodeSize.x + MIN_SPACING > group.position.x &&
                        nodePos.y < group.position.y + group.size.y + MIN_SPACING &&
                        nodePos.y + nodeSize.y + MIN_SPACING > group.position.y)
                    {
                        // Move node away from group (prefer moving right/down)
                        float deltaX = (group.position.x + group.size.x + MIN_SPACING) - nodePos.x;
                        float deltaY = (group.position.y + group.size.y + MIN_SPACING) - nodePos.y;
                        
                        if (std::abs(deltaX) < std::abs(deltaY))
                        {
                            node->screenPos().x += deltaX;
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
            for (auto* constantNode : constantNodes)
            {
                auto nodePos = constantNode->screenPos();
                auto nodeSize = ed::GetNodeSize(constantNode->getId());
                if (nodeSize.x <= 0.0f) nodeSize.x = 150.0f; // Constant nodes are usually smaller
                if (nodeSize.y <= 0.0f) nodeSize.y = 80.0f;

                for (auto& group : groups)
                {
                    // Check if constant node overlaps with group
                    if (nodePos.x < group.position.x + group.size.x + MIN_SPACING &&
                        nodePos.x + nodeSize.x + MIN_SPACING > group.position.x &&
                        nodePos.y < group.position.y + group.size.y + MIN_SPACING &&
                        nodePos.y + nodeSize.y + MIN_SPACING > group.position.y)
                    {
                        // Move constant node away from group (prefer moving left since constants are typically to the left)
                        float deltaX = group.position.x - (nodePos.x + nodeSize.x + MIN_SPACING);
                        float deltaY = (group.position.y + group.size.y + MIN_SPACING) - nodePos.y;
                        
                        if (std::abs(deltaX) < std::abs(deltaY) && deltaX < 0)
                        {
                            constantNode->screenPos().x += deltaX;
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
            for (auto* constantNode : constantNodes)
            {
                auto constantPos = constantNode->screenPos();
                auto constantSize = ed::GetNodeSize(constantNode->getId());
                if (constantSize.x <= 0.0f) constantSize.x = 150.0f;
                if (constantSize.y <= 0.0f) constantSize.y = 80.0f;

                for (auto* regularNode : ungroupedNodes)
                {
                    auto nodePos = regularNode->screenPos();
                    auto nodeSize = ed::GetNodeSize(regularNode->getId());
                    if (nodeSize.x <= 0.0f) nodeSize.x = 200.0f;
                    if (nodeSize.y <= 0.0f) nodeSize.y = 100.0f;

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
                            constantNode->screenPos().x += deltaX;
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
                auto* node1 = constantNodes[i];
                auto pos1 = node1->screenPos();
                auto size1 = ed::GetNodeSize(node1->getId());
                if (size1.x <= 0.0f) size1.x = 150.0f;
                if (size1.y <= 0.0f) size1.y = 80.0f;

                for (size_t j = i + 1; j < constantNodes.size(); ++j)
                {
                    auto* node2 = constantNodes[j];
                    auto pos2 = node2->screenPos();
                    auto size2 = ed::GetNodeSize(node2->getId());
                    if (size2.x <= 0.0f) size2.x = 150.0f;
                    if (size2.y <= 0.0f) size2.y = 80.0f;

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
                            node1->screenPos().y += -overlapY;
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

    std::unordered_map<nodes::NodeId, int> NodeLayoutEngine::determineDepth(
        const nodes::graph::IDirectedGraph& graph,
        nodes::NodeId beginId)
    {
        return nodes::graph::determineDepth(graph, beginId);
    }

    template<typename T>
    std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<T>*>> NodeLayoutEngine::arrangeInLayers(std::vector<LayoutEntity<T>>& entities)
    {
        std::map<int, std::vector<LayoutEntity<T>*>> layers;
        
        for (auto& entity : entities)
        {
            layers[entity.depth].push_back(&entity);
        }
        
        // Sort within each layer (for consistent ordering)
        for (auto& [depth, layerEntities] : layers)
        {
            std::sort(layerEntities.begin(), layerEntities.end(),
                     [](const LayoutEntity<T>* a, const LayoutEntity<T>* b) {
                         return a->item < b->item; // Pointer comparison for consistency
                     });
        }
        
        return layers;
    }

    template<typename T>
    void NodeLayoutEngine::optimizeLayerPositions(std::map<int, std::vector<LayoutEntity<T>*>>& layers, 
                                                  const LayoutConfig& config)
    {
        // Simple optimization: distribute entities evenly within each layer
        for (auto& [depth, layerEntities] : layers)
        {
            if (layerEntities.size() <= 1)
            {
                continue;
            }

            // Sort by current Y position
            std::sort(layerEntities.begin(), layerEntities.end(),
                     [](const LayoutEntity<T>* a, const LayoutEntity<T>* b) {
                         return a->position.y < b->position.y;
                     });

            // Redistribute with consistent spacing
            float currentY = 0.0f;
            for (auto* entity : layerEntities)
            {
                entity->position.y = currentY;
                currentY += entity->size.y + config.nodeDistance;
            }
        }
    }

    ImVec2 NodeLayoutEngine::calculateEntitySize(NodeEntity& entity)
    {
        auto nodeSize = ed::GetNodeSize(entity.item->getId());
        if (nodeSize.x <= 0.0f) nodeSize.x = 200.0f; // Default width
        if (nodeSize.y <= 0.0f) nodeSize.y = 100.0f; // Default height
        return nodeSize;
    }

    ImVec2 NodeLayoutEngine::calculateGroupSize(const GroupInfo& groupInfo)
    {
        if (groupInfo.nodes.empty())
        {
            return ImVec2(0, 0);
        }

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (auto* node : groupInfo.nodes)
        {
            auto pos = node->screenPos();
            auto size = ed::GetNodeSize(node->getId());
            if (size.x <= 0.0f) size.x = 200.0f;
            if (size.y <= 0.0f) size.y = 100.0f;

            minX = std::min(minX, pos.x);
            minY = std::min(minY, pos.y);
            maxX = std::max(maxX, pos.x + size.x);
            maxY = std::max(maxY, pos.y + size.y);
        }

        return ImVec2(maxX - minX, maxY - minY);
    }

    void NodeLayoutEngine::updateGroupBounds(GroupInfo& groupInfo)
    {
        if (groupInfo.nodes.empty())
        {
            return;
        }

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();

        for (auto* node : groupInfo.nodes)
        {
            auto pos = node->screenPos();
            auto size = ed::GetNodeSize(node->getId());
            if (size.x <= 0.0f) size.x = 200.0f;
            if (size.y <= 0.0f) size.y = 100.0f;

            minX = std::min(minX, pos.x);
            minY = std::min(minY, pos.y);
            maxX = std::max(maxX, pos.x + size.x);
            maxY = std::max(maxY, pos.y + size.y);
        }

        groupInfo.position = ImVec2(minX, minY);
        groupInfo.size = ImVec2(maxX - minX, maxY - minY);
    }

    // Template instantiations
    template void NodeLayoutEngine::performLayeredLayout<nodes::NodeBase>(std::vector<LayoutEntity<nodes::NodeBase>>&, const LayoutConfig&);
    template void NodeLayoutEngine::performLayeredLayout<std::vector<nodes::NodeBase*>>(std::vector<LayoutEntity<std::vector<nodes::NodeBase*>>>&, const LayoutConfig&);
    template std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<nodes::NodeBase>*>> NodeLayoutEngine::arrangeInLayers<nodes::NodeBase>(std::vector<LayoutEntity<nodes::NodeBase>>&);
    template std::map<int, std::vector<NodeLayoutEngine::LayoutEntity<std::vector<nodes::NodeBase*>>*>> NodeLayoutEngine::arrangeInLayers<std::vector<nodes::NodeBase*>>(std::vector<LayoutEntity<std::vector<nodes::NodeBase*>>>&);
    template void NodeLayoutEngine::optimizeLayerPositions<nodes::NodeBase>(std::map<int, std::vector<LayoutEntity<nodes::NodeBase>*>>&, const LayoutConfig&);
    template void NodeLayoutEngine::optimizeLayerPositions<std::vector<nodes::NodeBase*>>(std::map<int, std::vector<LayoutEntity<std::vector<nodes::NodeBase*>>*>>&, const LayoutConfig&);

} // namespace gladius::ui
