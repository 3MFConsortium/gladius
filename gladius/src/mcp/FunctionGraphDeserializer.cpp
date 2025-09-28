/**
 * @file FunctionGraphDeserializer.cpp
 */

#include "FunctionGraphDeserializer.h"

#include "../nodes/Model.h"
#include "../nodes/NodeFactory.h"
#include "../nodes/Parameter.h"
#include "../nodes/Port.h"
#include "../nodes/nodesfwd.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace gladius
{
    namespace mcp
    {
        using nlohmann::json;

        json FunctionGraphDeserializer::applyToModel(nodes::Model & model,
                                                     json const & graph,
                                                     bool replace)
        {
            // Validate input
            if (!graph.is_object())
            {
                return json{{"success", false}, {"error", "graph must be a JSON object"}};
            }
            if (!graph.contains("nodes") || !graph["nodes"].is_array())
            {
                return json{{"success", false}, {"error", "graph.nodes must be an array"}};
            }

            // Optionally clear existing graph
            if (replace)
            {
                model.clear();
                model.createBeginEndWithDefaultInAndOuts();
            }

            // Build mapping from client ids to actual NodeBase*
            std::unordered_map<uint32_t, nodes::NodeBase *> idMap;

            // Keep handles to Begin/End for special mapping
            auto * beginNode = model.getBeginNode();
            auto * endNode = model.getEndNode();

            // Track whether incoming graph provides meaningful layout positions
            bool anyPosition = false;
            bool allNearOrigin = true;
            float minX = std::numeric_limits<float>::max();
            float minY = std::numeric_limits<float>::max();
            float maxX = -std::numeric_limits<float>::max();
            float maxY = -std::numeric_limits<float>::max();

            // First pass: create nodes
            for (const auto & jn : graph["nodes"])
            {
                if (!jn.is_object())
                    continue;

                uint32_t clientId = jn.value("id", 0u);
                std::string type = jn.value("type", "");
                std::string displayName = jn.value("display_name", "");
                nodes::NodeBase * created = nullptr;

                // Map special types to existing begin/end
                if (type == "Input" || type == "Begin")
                {
                    created = beginNode;
                    if (!displayName.empty())
                        created->setDisplayName(displayName);
                }
                else if (type == "Output" || type == "End")
                {
                    created = endNode;
                    if (!displayName.empty())
                        created->setDisplayName(displayName);
                }
                else
                {
                    auto newNode = nodes::NodeFactory::createNode(type);
                    if (!newNode)
                    {
                        return json{{"success", false},
                                    {"error", std::string("Unknown node type: ") + type}};
                    }
                    if (!displayName.empty())
                        newNode->setDisplayName(displayName);
                    created = model.insert(std::move(newNode));
                }

                if (jn.contains("position") && jn["position"].is_array() &&
                    jn["position"].size() == 2)
                {
                    auto pos = const_cast<nodes::NodeBase *>(created)->screenPos();
                    pos.x = static_cast<float>(jn["position"][0].get<double>());
                    pos.y = static_cast<float>(jn["position"][1].get<double>());

                    anyPosition = true;
                    allNearOrigin = allNearOrigin && (std::abs(pos.x) < 1e-3f) && (std::abs(pos.y) < 1e-3f);
                    minX = std::min(minX, pos.x);
                    minY = std::min(minY, pos.y);
                    maxX = std::max(maxX, pos.x);
                    maxY = std::max(maxY, pos.y);
                }

                if (clientId != 0 && created)
                {
                    idMap[clientId] = created;
                }
            }

            // Update graph/ports prior to linking
            model.updateGraphAndOrderIfNeeded();

            // Second pass: create links
            if (graph.contains("links") && graph["links"].is_array())
            {
                for (const auto & jl : graph["links"])
                {
                    if (!jl.is_object())
                        continue;
                    uint32_t fromNodeId = jl.value("from_node_id", 0u);
                    uint32_t toNodeId = jl.value("to_node_id", 0u);
                    std::string fromPort = jl.value("from_port", "");
                    std::string toParam = jl.value("to_parameter", "");

                    if (!fromNodeId || !toNodeId || fromPort.empty() || toParam.empty())
                        continue;

                    auto itFrom = idMap.find(fromNodeId);
                    auto itTo = idMap.find(toNodeId);
                    if (itFrom == idMap.end() || itTo == idMap.end())
                        continue;

                    nodes::NodeBase * srcNode = itFrom->second;
                    nodes::NodeBase * dstNode = itTo->second;
                    auto * port = srcNode->findOutputPort(fromPort);
                    auto * param = dstNode->getParameter(toParam);
                    if (!port || !param)
                        continue;

                    // Ensure port has parent id set and ids are valid
                    model.registerOutput(*port);
                    model.registerInput(*param);
                    model.addLink(port->getId(), param->getId());
                }
            }

            // Finalize
            model.updateGraphAndOrderIfNeeded();

            if (anyPosition)
            {
                float const spanX = maxX - minX;
                float const spanY = maxY - minY;
                bool const degenerateSpan = (std::abs(spanX) < 1e-3f) && (std::abs(spanY) < 1e-3f);
                if (!(allNearOrigin || degenerateSpan))
                {
                    model.markAsLayouted();
                }
            }

            json out;
            out["success"] = true;
            json jmap = json::object();
            for (const auto & [cid, node] : idMap)
            {
                jmap[std::to_string(cid)] = node->getId();
            }
            out["id_map"] = jmap;
            return out;
        }
    } // namespace mcp
} // namespace gladius
