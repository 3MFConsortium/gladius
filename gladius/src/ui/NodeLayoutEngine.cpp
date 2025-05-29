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

        auto getDepth = [&](nodes::NodeId nodeId)
        {
            auto const depthIter = depthMap.find(nodeId);
            if (depthIter != std::end(depthMap))
            {
                return depthIter->second;
            }
            return 0;
        };

        auto getDepthCloseToSuccessor = [&](nodes::NodeId nodeId)
        {
            auto successsor = nodes::graph::determineSuccessor(graph, nodeId);
            int lowestDepth = std::numeric_limits<int>::max();
            for (auto const succ : successsor)
            {
                auto const depthIter = depthMap.find(succ);
                if (depthIter != std::end(depthMap))
                {
                    lowestDepth = std::min(lowestDepth, depthIter->second);
                }
            }

            if (lowestDepth != std::numeric_limits<int>::max())
            {
                return lowestDepth - 1;
            }
            return 0;
        };

        auto determineDepthForNode = [&](nodes::NodeId nodeId)
        {
            auto const depth = getDepth(nodeId);
            if (depth == 0)
            {
                return getDepthCloseToSuccessor(nodeId);
            }
            return depth;
        };

        // Step 1: Assign Layers (with group analysis)
        std::map<int, std::vector<nodes::NodeBase *>> layers;
        std::map<int, float> layersWidth;
        
        // Store original individual depths for within-group sorting
        std::unordered_map<nodes::NodeId, int> originalIndividualDepths;
        for (auto& [nodeId, depth] : depthMap)
        {
            originalIndividualDepths[nodeId] = depth;
        }
        
        // Analyze group constraints before layer assignment
        auto groupInfos = analyzeGroupDepthConstraints(depthMap, model);
        auto groupPlacements = optimizeGroupPlacements(groupInfos, layers);
        
        // Create a map from node ID to preferred depth based on group constraints
        std::unordered_map<nodes::NodeId, int> nodeToPreferredDepth;
        for (const auto& placement : groupPlacements)
        {
            auto groupInfoIt = std::find_if(groupInfos.begin(), groupInfos.end(),
                [&placement](const GroupDepthInfo& info) { return info.tag == placement.tag; });
            if (groupInfoIt != groupInfos.end())
            {
                for (nodes::NodeId nodeId : groupInfoIt->nodeIds)
                {
                    nodeToPreferredDepth[nodeId] = placement.chosenDepth;
                }
            }
        }
        
        // Assign nodes to layers, considering group preferences
        for (auto & [id, node] : model)
        {
            int depth;
            auto preferredIt = nodeToPreferredDepth.find(id);
            if (preferredIt != nodeToPreferredDepth.end())
            {
                depth = preferredIt->second;
            }
            else
            {
                depth = (id == beginId) ? 0 : determineDepthForNode(id);
            }
            
            layers[depth].push_back(node.get());
            auto const nodeWidth = ed::GetNodeSize(node->getId()).x;
            layersWidth[depth] = std::max(layersWidth[depth], nodeWidth);
        }

        // Step 2: Order Nodes within Layers (group-aware clustering)
        for (auto & [depth, nodes] : layers)
        {
            std::sort(nodes.begin(),
                      nodes.end(),
                      [](nodes::NodeBase * a, nodes::NodeBase * b)
                      {
                          const std::string& tagA = a->getTag();
                          const std::string& tagB = b->getTag();
                          
                          if (tagA != tagB)
                          {
                              if (tagA.empty()) return false;
                              if (tagB.empty()) return true;
                              return tagA < tagB;
                          }
                          
                          return a->getOrder() < b->getOrder();
                      });
        }

        // Calculate layer X positions
        std::map<int, float> layerXPositions;
        float x = 0.f;
        for (auto & [depth, width] : layersWidth)
        {
            layerXPositions[depth] = x;
            x += width + config.nodeDistance;
        }

        // Step 3: Apply group-aware coordinate assignment
        applyGroupAwareCoordinates(layers, layerXPositions, originalIndividualDepths, config);
        
        // Step 4: Resolve any group overlaps
        resolveGroupOverlaps(layers, config);

        model.markAsLayouted();
    }

    std::vector<NodeLayoutEngine::GroupDepthInfo> NodeLayoutEngine::analyzeGroupDepthConstraints(
        const std::unordered_map<nodes::NodeId, int>& depthMap,
        nodes::Model& model)
    {
        std::unordered_map<std::string, GroupDepthInfo> groupMap;
        
        // Collect nodes by group
        for (auto & [id, node] : model)
        {
            const std::string& tag = node->getTag();
            if (tag.empty()) continue;
            
            auto depth = depthMap.find(id);
            int nodeDepth = (depth != depthMap.end()) ? depth->second : 0;
            
            if (groupMap.find(tag) == groupMap.end())
            {
                groupMap[tag] = {
                    tag,
                    nodeDepth,
                    nodeDepth,
                    {id},
                    true
                };
            }
            else
            {
                auto& groupInfo = groupMap[tag];
                groupInfo.minRequiredDepth = std::min(groupInfo.minRequiredDepth, nodeDepth);
                groupInfo.maxRequiredDepth = std::max(groupInfo.maxRequiredDepth, nodeDepth);
                groupInfo.nodeIds.push_back(id);
                
                // Group can be moved together if depth range is reasonable
                groupInfo.canBeMovedTogether = 
                    (groupInfo.maxRequiredDepth - groupInfo.minRequiredDepth) <= 2;
            }
        }
        
        std::vector<GroupDepthInfo> result;
        for (auto& [tag, info] : groupMap)
        {
            result.push_back(std::move(info));
        }
        
        return result;
    }

    std::vector<NodeLayoutEngine::GroupPlacementOption> NodeLayoutEngine::optimizeGroupPlacements(
        const std::vector<GroupDepthInfo>& groupInfos,
        std::map<int, std::vector<nodes::NodeBase*>>& layers)
    {
        std::vector<GroupPlacementOption> placements;
        
        for (const auto& groupInfo : groupInfos)
        {
            int bestDepth = groupInfo.minRequiredDepth;
            float bestCost = std::numeric_limits<float>::max();
            
            // Try different depth options for this group
            for (int depth = groupInfo.minRequiredDepth; 
                 depth <= groupInfo.maxRequiredDepth + 1; 
                 ++depth)
            {
                float cost = calculateGroupPlacementCost(groupInfo, depth, layers);
                if (cost < bestCost)
                {
                    bestCost = cost;
                    bestDepth = depth;
                }
            }
            
            placements.push_back({groupInfo.tag, bestDepth, bestCost});
        }
        
        return placements;
    }

    float NodeLayoutEngine::calculateGroupPlacementCost(
        const GroupDepthInfo& groupInfo,
        int targetDepth,
        const std::map<int, std::vector<nodes::NodeBase*>>& layers)
    {
        float cost = 0.0f;
        
        // Penalty for fragmenting the group across multiple layers
        if (targetDepth < groupInfo.minRequiredDepth || targetDepth > groupInfo.maxRequiredDepth)
        {
            cost += 100.0f; // High penalty for invalid placement
        }
        
        // Penalty for placing in crowded layers
        auto layerIt = layers.find(targetDepth);
        if (layerIt != layers.end())
        {
            cost += static_cast<float>(layerIt->second.size()) * 10.0f;
        }
        
        // Bonus for keeping group compact
        if (groupInfo.canBeMovedTogether)
        {
            cost -= 20.0f;
        }
        
        return cost;
    }

    void NodeLayoutEngine::applyGroupAwareCoordinates(
        std::map<int, std::vector<nodes::NodeBase*>>& layers,
        const std::map<int, float>& layerXPositions,
        const std::unordered_map<nodes::NodeId, int>& originalIndividualDepths,
        const LayoutConfig& config)
    {
        // Create a mapping from nodes to their actual topological depths
        std::map<nodes::NodeBase*, int> nodeToActualDepth;
        
        // First, identify the global min and max topological depths
        int minTopologicalDepth = std::numeric_limits<int>::max();
        int maxTopologicalDepth = std::numeric_limits<int>::min();
        
        for (auto& [layer, nodes] : layers)
        {
            for (auto* node : nodes)
            {
                int topologicalDepth = originalIndividualDepths.count(node->getId()) ? 
                    originalIndividualDepths.at(node->getId()) : 0;
                nodeToActualDepth[node] = topologicalDepth;
                minTopologicalDepth = std::min(minTopologicalDepth, topologicalDepth);
                maxTopologicalDepth = std::max(maxTopologicalDepth, topologicalDepth);
            }
        }
        
        // Create a mapping from topological depth to nodes (grouped by tags)
        std::map<int, std::map<std::string, std::vector<nodes::NodeBase*>>> depthToTaggedNodes;
        std::map<int, std::vector<nodes::NodeBase*>> depthToUngroupedNodes;
        
        // Organize nodes by their topological depth and groups
        for (auto& [layer, nodes] : layers)
        {
            for (auto* node : nodes)
            {
                int topologicalDepth = nodeToActualDepth[node];
                const std::string& tag = node->getTag();
                
                if (tag.empty())
                {
                    depthToUngroupedNodes[topologicalDepth].push_back(node);
                }
                else
                {
                    depthToTaggedNodes[topologicalDepth][tag].push_back(node);
                }
            }
        }
        
        // Find all unique groups
        std::set<std::string> allGroupTags;
        for (auto& [depth, taggedNodes] : depthToTaggedNodes)
        {
            for (auto& [tag, nodes] : taggedNodes)
            {
                allGroupTags.insert(tag);
            }
        }
        
        // For each group, find min and max topological depths
        std::map<std::string, std::pair<int, int>> groupDepthRanges;
        for (const auto& tag : allGroupTags)
        {
            int minDepth = std::numeric_limits<int>::max();
            int maxDepth = std::numeric_limits<int>::min();
            
            for (auto& [depth, taggedNodes] : depthToTaggedNodes)
            {
                if (taggedNodes.find(tag) != taggedNodes.end())
                {
                    minDepth = std::min(minDepth, depth);
                    maxDepth = std::max(maxDepth, depth);
                }
            }
            
            groupDepthRanges[tag] = {minDepth, maxDepth};
        }
        
        // Now create topological columns - nodes in a column share the same topological depth
        // Each column may contain ungrouped nodes and nodes from different groups
        const float columnSpacing = config.nodeDistance * 1.5f;
        float xPos = 0.0f;
        std::map<int, float> depthToXPosition;
        
        // Assign X positions based on topological depth
        for (int depth = minTopologicalDepth; depth <= maxTopologicalDepth; ++depth)
        {
            depthToXPosition[depth] = xPos;
            
            // Find max width among all nodes at this depth
            float maxWidth = 0.0f;
            
            // Check ungrouped nodes
            if (depthToUngroupedNodes.find(depth) != depthToUngroupedNodes.end())
            {
                for (auto* node : depthToUngroupedNodes[depth])
                {
                    float nodeWidth = ed::GetNodeSize(node->getId()).x;
                    if (nodeWidth <= 0.0f)
                    {
                        nodeWidth = 200.0f; // Default width
                    }
                    maxWidth = std::max(maxWidth, nodeWidth);
                }
            }
            
            // Check grouped nodes
            if (depthToTaggedNodes.find(depth) != depthToTaggedNodes.end())
            {
                for (auto& [tag, nodes] : depthToTaggedNodes[depth])
                {
                    for (auto* node : nodes)
                    {
                        float nodeWidth = ed::GetNodeSize(node->getId()).x;
                        if (nodeWidth <= 0.0f)
                        {
                            nodeWidth = 200.0f; // Default width
                        }
                        maxWidth = std::max(maxWidth, nodeWidth);
                    }
                }
            }
            
            // Update X position for next column
            xPos += maxWidth + columnSpacing;
        }
        
        // Now position all nodes by their topological depth
        // First, position ungrouped nodes
        std::map<int, float> depthToYPosition;
        for (int depth = minTopologicalDepth; depth <= maxTopologicalDepth; ++depth)
        {
            float yPos = 0.0f;
            
            if (depthToUngroupedNodes.find(depth) != depthToUngroupedNodes.end())
            {
                for (auto* node : depthToUngroupedNodes[depth])
                {
                    auto& pos = node->screenPos();
                    auto nodeHeight = ed::GetNodeSize(node->getId()).y;
                    
                    if (nodeHeight <= 0.0f)
                    {
                        bool isResourceNode = dynamic_cast<nodes::Resource*>(node) != nullptr;
                        if (isResourceNode)
                        {
                            continue;
                        }
                        nodeHeight = 100.0f; // Default height
                    }
                    
                    pos.x = depthToXPosition[depth];
                    pos.y = yPos;
                    yPos += nodeHeight + config.nodeDistance;
                }
            }
            
            depthToYPosition[depth] = yPos + config.nodeDistance * 2.0f; // Add extra space for groups below
        }
        
        // Now position grouped nodes
        for (const auto& tag : allGroupTags)
        {
            int minDepth = groupDepthRanges[tag].first;
            int maxDepth = groupDepthRanges[tag].second;
            
            // Position nodes in this group across their depths
            std::map<int, float> groupDepthToYPosition;
            
            for (int depth = minDepth; depth <= maxDepth; ++depth)
            {
                if (depthToTaggedNodes.find(depth) != depthToTaggedNodes.end() &&
                    depthToTaggedNodes[depth].find(tag) != depthToTaggedNodes[depth].end())
                {
                    float yPos = depthToYPosition[depth]; // Start below ungrouped nodes
                    
                    for (auto* node : depthToTaggedNodes[depth][tag])
                    {
                        auto& pos = node->screenPos();
                        auto nodeHeight = ed::GetNodeSize(node->getId()).y;
                        
                        if (nodeHeight <= 0.0f)
                        {
                            bool isResourceNode = dynamic_cast<nodes::Resource*>(node) != nullptr;
                            if (isResourceNode)
                            {
                                continue;
                            }
                            nodeHeight = 100.0f; // Default height
                        }
                        
                        pos.x = depthToXPosition[depth];
                        pos.y = yPos;
                        yPos += nodeHeight + config.nodeDistance;
                    }
                    
                    // Update the Y position for the next group at this depth
                    depthToYPosition[depth] = yPos + config.nodeDistance;
                }
            }
        }
    }

    void NodeLayoutEngine::resolveGroupOverlaps(
        std::map<int, std::vector<nodes::NodeBase*>>& layers,
        const LayoutConfig& config)
    {
        struct NodeBounds
        {
            ImVec2 minBound;
            ImVec2 maxBound;
            
            bool overlaps(const NodeBounds& other, float minSpacing) const
            {
                return !(maxBound.x + minSpacing <= other.minBound.x || 
                        minBound.x >= other.maxBound.x + minSpacing ||
                        maxBound.y + minSpacing <= other.minBound.y || 
                        minBound.y >= other.maxBound.y + minSpacing);
            }
        };
        
        struct GroupBounds : NodeBounds
        {
            std::string tag;
            std::vector<nodes::NodeBase*> nodes;
        };
        
        std::unordered_map<std::string, GroupBounds> groupBounds;
        std::vector<std::pair<NodeBounds, nodes::NodeBase*>> ungroupedNodeBounds;
        
        constexpr float GROUP_PADDING = 40.0f;
        constexpr float NODE_PADDING = 10.0f;
        
        // Calculate current bounds for each group and ungrouped node
        for (const auto& [depth, nodes] : layers)
        {
            for (auto* node : nodes)
            {
                auto& pos = node->screenPos();
                auto nodeSize = ed::GetNodeSize(node->getId());
                
                if (nodeSize.x <= 0.0f || nodeSize.y <= 0.0f)
                {
                    nodeSize = ImVec2(200.0f, 100.0f);
                }
                
                const std::string& tag = node->getTag();
                if (tag.empty())
                {
                    // This is an ungrouped node
                    NodeBounds nodeBound;
                    nodeBound.minBound = ImVec2(pos.x, pos.y);
                    nodeBound.maxBound = ImVec2(pos.x + nodeSize.x, pos.y + nodeSize.y);
                    ungroupedNodeBounds.push_back({nodeBound, node});
                }
                else
                {
                    // This is a grouped node
                    if (groupBounds.find(tag) == groupBounds.end())
                    {
                        GroupBounds newBound;
                        newBound.tag = tag;
                        newBound.minBound = ImVec2(pos.x - GROUP_PADDING, pos.y - GROUP_PADDING);
                        newBound.maxBound = ImVec2(pos.x + nodeSize.x + GROUP_PADDING, pos.y + nodeSize.y + GROUP_PADDING);
                        newBound.nodes.push_back(node);
                        groupBounds[tag] = newBound;
                    }
                    else
                    {
                        auto& bounds = groupBounds[tag];
                        bounds.minBound.x = std::min(bounds.minBound.x, pos.x - GROUP_PADDING);
                        bounds.minBound.y = std::min(bounds.minBound.y, pos.y - GROUP_PADDING);
                        bounds.maxBound.x = std::max(bounds.maxBound.x, pos.x + nodeSize.x + GROUP_PADDING);
                        bounds.maxBound.y = std::max(bounds.maxBound.y, pos.y + nodeSize.y + GROUP_PADDING);
                        bounds.nodes.push_back(node);
                    }
                }
            }
        }
        
        // Resolve overlaps iteratively
        bool hasOverlaps = true;
        int iteration = 0;
        
        while (hasOverlaps && iteration < config.maxOverlapResolutionIterations)
        {
            hasOverlaps = false;
            ++iteration;
            
            // First, resolve overlaps between groups
            for (auto& [tag1, bounds1] : groupBounds)
            {
                for (auto& [tag2, bounds2] : groupBounds)
                {
                    if (tag1 >= tag2) continue;
                    
                    if (bounds1.overlaps(bounds2, config.minGroupSpacing))
                    {
                        hasOverlaps = true;
                        
                        float horizontalOverlap = std::min(bounds1.maxBound.x, bounds2.maxBound.x) - 
                                                 std::max(bounds1.minBound.x, bounds2.minBound.x);
                        float verticalOverlap = std::min(bounds1.maxBound.y, bounds2.maxBound.y) - 
                                               std::max(bounds1.minBound.y, bounds2.minBound.y);
                        
                        if (horizontalOverlap < verticalOverlap)
                        {
                            if (bounds1.minBound.x < bounds2.minBound.x)
                            {
                                float moveDistance = bounds1.maxBound.x + config.minGroupSpacing - bounds2.minBound.x;
                                for (auto* node : bounds2.nodes)
                                {
                                    node->screenPos().x += moveDistance;
                                }
                                bounds2.minBound.x += moveDistance;
                                bounds2.maxBound.x += moveDistance;
                            }
                            else
                            {
                                float moveDistance = bounds2.maxBound.x + config.minGroupSpacing - bounds1.minBound.x;
                                for (auto* node : bounds1.nodes)
                                {
                                    node->screenPos().x += moveDistance;
                                }
                                bounds1.minBound.x += moveDistance;
                                bounds1.maxBound.x += moveDistance;
                            }
                        }
                        else
                        {
                            if (bounds1.minBound.y < bounds2.minBound.y)
                            {
                                float moveDistance = bounds1.maxBound.y + config.minGroupSpacing - bounds2.minBound.y;
                                for (auto* node : bounds2.nodes)
                                {
                                    node->screenPos().y += moveDistance;
                                }
                                bounds2.minBound.y += moveDistance;
                                bounds2.maxBound.y += moveDistance;
                            }
                            else
                            {
                                float moveDistance = bounds2.maxBound.y + config.minGroupSpacing - bounds1.minBound.y;
                                for (auto* node : bounds1.nodes)
                                {
                                    node->screenPos().y += moveDistance;
                                }
                                bounds1.minBound.y += moveDistance;
                                bounds1.maxBound.y += moveDistance;
                            }
                        }
                    }
                }
            }
            
            // Now resolve overlaps between ungrouped nodes and groups
            for (auto& [nodeBound, node] : ungroupedNodeBounds)
            {
                for (auto& [tag, groupBound] : groupBounds)
                {
                    if (nodeBound.overlaps(groupBound, NODE_PADDING))
                    {
                        hasOverlaps = true;
                        
                        // Update node bounds to current position (might have been moved in previous iterations)
                        auto& pos = node->screenPos();
                        auto nodeSize = ed::GetNodeSize(node->getId());
                        if (nodeSize.x <= 0.0f || nodeSize.y <= 0.0f)
                        {
                            nodeSize = ImVec2(200.0f, 100.0f);
                        }
                        nodeBound.minBound = ImVec2(pos.x, pos.y);
                        nodeBound.maxBound = ImVec2(pos.x + nodeSize.x, pos.y + nodeSize.y);
                        
                        // Move the ungrouped node outside the group
                        // Determine shortest direction to move
                        float distToLeft = nodeBound.minBound.x - groupBound.minBound.x;
                        float distToRight = groupBound.maxBound.x - nodeBound.maxBound.x;
                        float distToTop = nodeBound.minBound.y - groupBound.minBound.y;
                        float distToBottom = groupBound.maxBound.y - nodeBound.maxBound.y;
                        
                        float minDist = std::min({std::abs(distToLeft), std::abs(distToRight), 
                                               std::abs(distToTop), std::abs(distToBottom)});
                        
                        if (minDist == std::abs(distToLeft) && distToLeft < 0)
                        {
                            // Move to the left of the group
                            float moveDistance = groupBound.minBound.x - nodeBound.maxBound.x - NODE_PADDING;
                            pos.x += moveDistance;
                            nodeBound.minBound.x += moveDistance;
                            nodeBound.maxBound.x += moveDistance;
                        }
                        else if (minDist == std::abs(distToRight) && distToRight < 0)
                        {
                            // Move to the right of the group
                            float moveDistance = groupBound.maxBound.x - nodeBound.minBound.x + NODE_PADDING;
                            pos.x += moveDistance;
                            nodeBound.minBound.x += moveDistance;
                            nodeBound.maxBound.x += moveDistance;
                        }
                        else if (minDist == std::abs(distToTop) && distToTop < 0)
                        {
                            // Move above the group
                            float moveDistance = groupBound.minBound.y - nodeBound.maxBound.y - NODE_PADDING;
                            pos.y += moveDistance;
                            nodeBound.minBound.y += moveDistance;
                            nodeBound.maxBound.y += moveDistance;
                        }
                        else if (minDist == std::abs(distToBottom) && distToBottom < 0)
                        {
                            // Move below the group
                            float moveDistance = groupBound.maxBound.y - nodeBound.minBound.y + NODE_PADDING;
                            pos.y += moveDistance;
                            nodeBound.minBound.y += moveDistance;
                            nodeBound.maxBound.y += moveDistance;
                        }
                    }
                }
            }
            
            // Finally resolve overlaps between ungrouped nodes
            for (size_t i = 0; i < ungroupedNodeBounds.size(); i++)
            {
                auto& [boundsA, nodeA] = ungroupedNodeBounds[i];
                
                for (size_t j = i + 1; j < ungroupedNodeBounds.size(); j++)
                {
                    auto& [boundsB, nodeB] = ungroupedNodeBounds[j];
                    
                    if (boundsA.overlaps(boundsB, NODE_PADDING))
                    {
                        hasOverlaps = true;
                        
                        // Update node bounds to current positions
                        auto& posA = nodeA->screenPos();
                        auto sizeA = ed::GetNodeSize(nodeA->getId());
                        if (sizeA.x <= 0.0f || sizeA.y <= 0.0f)
                        {
                            sizeA = ImVec2(200.0f, 100.0f);
                        }
                        boundsA.minBound = ImVec2(posA.x, posA.y);
                        boundsA.maxBound = ImVec2(posA.x + sizeA.x, posA.y + sizeA.y);
                        
                        auto& posB = nodeB->screenPos();
                        auto sizeB = ed::GetNodeSize(nodeB->getId());
                        if (sizeB.x <= 0.0f || sizeB.y <= 0.0f)
                        {
                            sizeB = ImVec2(200.0f, 100.0f);
                        }
                        boundsB.minBound = ImVec2(posB.x, posB.y);
                        boundsB.maxBound = ImVec2(posB.x + sizeB.x, posB.y + sizeB.y);
                        
                        // Resolve overlap using similar logic to group-group overlap
                        float horizontalOverlap = std::min(boundsA.maxBound.x, boundsB.maxBound.x) - 
                                                std::max(boundsA.minBound.x, boundsB.minBound.x);
                        float verticalOverlap = std::min(boundsA.maxBound.y, boundsB.maxBound.y) - 
                                              std::max(boundsA.minBound.y, boundsB.minBound.y);
                        
                        if (horizontalOverlap < verticalOverlap)
                        {
                            if (boundsA.minBound.x < boundsB.minBound.x)
                            {
                                float moveDistance = boundsA.maxBound.x + NODE_PADDING - boundsB.minBound.x;
                                posB.x += moveDistance;
                                boundsB.minBound.x += moveDistance;
                                boundsB.maxBound.x += moveDistance;
                            }
                            else
                            {
                                float moveDistance = boundsB.maxBound.x + NODE_PADDING - boundsA.minBound.x;
                                posA.x += moveDistance;
                                boundsA.minBound.x += moveDistance;
                                boundsA.maxBound.x += moveDistance;
                            }
                        }
                        else
                        {
                            if (boundsA.minBound.y < boundsB.minBound.y)
                            {
                                float moveDistance = boundsA.maxBound.y + NODE_PADDING - boundsB.minBound.y;
                                posB.y += moveDistance;
                                boundsB.minBound.y += moveDistance;
                                boundsB.maxBound.y += moveDistance;
                            }
                            else
                            {
                                float moveDistance = boundsB.maxBound.y + NODE_PADDING - boundsA.minBound.y;
                                posA.y += moveDistance;
                                boundsA.minBound.y += moveDistance;
                                boundsA.maxBound.y += moveDistance;
                            }
                        }
                    }
                }
            }
        }
    }

    std::unordered_map<nodes::NodeId, int> NodeLayoutEngine::determineDepth(
        const nodes::graph::IDirectedGraph& graph,
        nodes::NodeId beginId)
    {
        // Use existing GraphAlgorithms function
        return nodes::graph::determineDepth(graph, beginId);
    }

} // namespace gladius::ui
