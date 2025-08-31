/**
 * @file FunctionOperationsTool.h
 * @brief Tool for function creation and manipulation operations
 */

#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "../FunctionArgument.h"
#include "MCPToolBase.h"

namespace gladius
{
    class Application; // Forward declaration

    namespace mcp::tools
    {
        /**
         * @brief Tool for function creation and manipulation operations
         *
         * Handles:
         * - Function creation from expressions
         * - SDF function creation
         * - CSG operations
         * - Function transformations
         * - Function analysis and mesh generation
         * - Function listing
         */
        class FunctionOperationsTool : public MCPToolBase
        {
          public:
            explicit FunctionOperationsTool(Application * app);
            ~FunctionOperationsTool() override = default;

            // Function creation and manipulation
            std::pair<bool, uint32_t>
            createFunctionFromExpression(const std::string & name,
                                         const std::string & expression,
                                         const std::string & outputType,
                                         const std::vector<FunctionArgument> & arguments = {},
                                         const std::string & outputName = "");

            // Function analysis
            nlohmann::json analyzeFunctionProperties(const std::string & functionName) const;

            nlohmann::json generateMeshFromFunction(
              const std::string & functionName,
              int resolution = 64,
              const std::array<float, 6> & bounds = {-10, -10, -10, 10, 10, 10}) const;

            // Function listing
            std::vector<std::string> listAvailableFunctions() const;

            // Node graph operations (could be moved to separate tool later)
            nlohmann::json getFunctionGraph(uint32_t functionId) const;
            nlohmann::json getNodeInfo(uint32_t functionId, uint32_t nodeId) const;
            nlohmann::json createNode(uint32_t functionId,
                                      const std::string & nodeType,
                                      const std::string & displayName,
                                      uint32_t nodeId);
            nlohmann::json deleteNode(uint32_t functionId, uint32_t nodeId);
            nlohmann::json setParameterValue(uint32_t functionId,
                                             uint32_t nodeId,
                                             const std::string & parameterName,
                                             const nlohmann::json & value);
            nlohmann::json createLink(uint32_t functionId,
                                      uint32_t sourceNodeId,
                                      const std::string & sourcePortName,
                                      uint32_t targetNodeId,
                                      const std::string & targetParameterName);
            nlohmann::json deleteLink(uint32_t functionId,
                                      uint32_t targetNodeId,
                                      const std::string & targetParameterName);
            nlohmann::json createFunctionCallNode(uint32_t targetFunctionId,
                                                  uint32_t referencedFunctionId,
                                                  const std::string & displayName = "");
            nlohmann::json createConstantNodesForMissingParameters(uint32_t functionId,
                                                                   uint32_t nodeId,
                                                                   bool autoConnect = true);
        };
    }
}
