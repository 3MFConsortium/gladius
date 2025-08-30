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
        static const char * typeIndexToString(std::type_index idx);
    };
}
