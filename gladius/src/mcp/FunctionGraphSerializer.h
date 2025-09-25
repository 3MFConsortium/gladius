#pragma once

#include "../nodes/Model.h"
#include <nlohmann/json.hpp>

namespace gladius
{
    /**
     * @brief Serializes a nodes::Model (function graph) into a stable JSON structure.
     *
     * The JSON contains nodes, ports, parameters, and links (wiring) so that
     * external tools can introspect or visualize the function graph.
     */
    class FunctionGraphSerializer
    {
      public:
        /**
         * @brief Serialize the given model to JSON.
         * @param model The model to serialize.
         * @return nlohmann::json representation of the model graph.
         */
        static nlohmann::json serialize(const nodes::Model & model);

        /**
         * @brief Serialize the given model to a minimal JSON suitable for MCP clients.
         * This omits internal-only fields and keeps just the essentials:
         * - model: resource_id, display_name (if any), name
         * - nodes: id, type, display_name, position, parameters (name,type,is_connected,source),
         * outputs (name,type)
         * - links: from_node_id, from_port, to_node_id, to_parameter, type
         * - counts: nodes, links
         */
        static nlohmann::json serializeMinimal(const nodes::Model & model);
        static const char * typeIndexToString(std::type_index idx);
    };
}
