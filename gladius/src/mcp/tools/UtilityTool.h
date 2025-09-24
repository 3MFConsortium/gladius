/**
 * @file UtilityTool.h
 * @brief Header for UtilityTool - handles utility operations
 */

#pragma once

#include "MCPToolBase.h"
#include <nlohmann/json.hpp>

namespace gladius
{
    class Application; // Forward declaration

    namespace mcp::tools
    {
        /**
         * @brief Tool for utility operations
         *
         * Handles batch operations and other utility functions.
         */
        class UtilityTool : public MCPToolBase
        {
          public:
            /**
             * @brief Construct a new UtilityTool object
             * @param app Pointer to the Application instance
             */
            explicit UtilityTool(Application * app);

            /**
             * @brief Execute batch operations
             * @param operations JSON array of operations to execute
             * @param rollbackOnError Whether to rollback on error
             * @return true if all operations succeeded
             */
            bool executeBatchOperations(const nlohmann::json & operations,
                                        bool rollbackOnError = true);
        };
    } // namespace tools
} // namespace gladius
