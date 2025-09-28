#pragma once

#include "Model.h"
#include "DerivedNodes.h"
#include <set>
#include <string>
#include <unordered_map>

namespace gladius::nodes
{
    /**
     * @brief Refactoring utility that extracts a selection of nodes from a source model
     *        into a new function model and replaces them with a FunctionCall in the source.
     *
     * Contract:
     * - Input: source model with a non-empty selection of nodes (excluding Begin/End)
     * - Output: new model populated with the extracted subgraph, Begin/End wired
     *           FunctionCall inserted into the source model, all links rewired.
     * - Error modes: returns false if selection invalid or rewiring fails; leaves models unchanged.
     */
    class FunctionExtractor
    {
      public:
        struct Result
        {
            FunctionCall * functionCall{nullptr};
            // Map of original external input source uniqueName -> function input name
            std::unordered_map<std::string, std::string> inputNameMap;
            // Map of original selected source port uniqueName -> function output name
            std::unordered_map<std::string, std::string> outputNameMap;
        };

        /**
         * @brief Extract the given selection into the provided newModel.
         *
         * Preconditions:
         * - newModel is an empty model or at least contains Begin/End; it will be populated.
         * - selection must not include Begin/End nodes of source.
         * - newModel's display name should be set by the caller as desired.
         *
         * @param sourceModel The model to extract from (modified in-place)
         * @param newModel The model to extract into (populated)
         * @param selection Node IDs (from sourceModel) to extract
         * @param outResult Filled with details and pointer to created FunctionCall in source
         * @return true on success, false otherwise
         */
        static bool extractInto(nodes::Model & sourceModel,
                                 nodes::Model & newModel,
                                 std::set<nodes::NodeId> const & selection,
                                 Result & outResult);

      private:
        static std::string makeUnique(std::string base, std::unordered_set<std::string> & used);
    };
} // namespace gladius::nodes
