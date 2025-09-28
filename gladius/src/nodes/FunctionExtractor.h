#pragma once

#include "DerivedNodes.h"
#include "Model.h"
#include <set>
#include <string>
#include <typeindex>
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
        struct NameProposalEntry
        {
            std::string uniqueKey;   ///< Stable key (e.g., source port unique name)
            std::string defaultName; ///< Suggested, human-friendly name
            std::type_index type;    ///< Port/parameter type
        };

        struct Proposals
        {
            std::vector<NameProposalEntry> inputs;  ///< Proposed function arguments
            std::vector<NameProposalEntry> outputs; ///< Proposed function outputs
        };

        struct Result
        {
            FunctionCall * functionCall{nullptr};
            // Map of original external input source uniqueName -> function input name
            std::unordered_map<std::string, std::string> inputNameMap;
            // Map of original selected source port uniqueName -> function output name
            std::unordered_map<std::string, std::string> outputNameMap;
        };

        /**
         * @brief Analyze selection to propose argument and output names without modifying models.
         * @param sourceModel The source model containing the selection
         * @param selection Node IDs (from sourceModel) to analyze
         * @return Proposals with stable keys and human-friendly default names
         */
        static Proposals proposeNames(nodes::Model & sourceModel,
                                      std::set<nodes::NodeId> const & selection);

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

        /**
         * @brief Extract with explicit name overrides for inputs/outputs.
         * If an override for a key is missing, a unique name will be generated.
         * @param sourceModel The model to extract from (modified)
         * @param newModel Destination model (populated)
         * @param selection Node IDs to extract
         * @param inputNameOverrides Map: external source uniqueName -> desired argument name
         * @param outputNameOverrides Map: selected source port uniqueName -> desired output name
         * @param outResult Filled with details and pointer to created FunctionCall in source
         */
        static bool
        extractInto(nodes::Model & sourceModel,
                    nodes::Model & newModel,
                    std::set<nodes::NodeId> const & selection,
                    std::unordered_map<std::string, std::string> const & inputNameOverrides,
                    std::unordered_map<std::string, std::string> const & outputNameOverrides,
                    Result & outResult);

      private:
        static std::string makeUnique(std::string base, std::unordered_set<std::string> & used);
    };
} // namespace gladius::nodes
