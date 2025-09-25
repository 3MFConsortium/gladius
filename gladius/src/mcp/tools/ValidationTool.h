/**
 * @file ValidationTool.h
 * @brief Header for ValidationTool - handles model validation operations
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
         * @brief Tool for model validation operations
         *
         * Handles two-phase validation (graph sync + OpenCL compile) and
         * manufacturing validation for 3MF models.
         */
        class ValidationTool : public MCPToolBase
        {
          public:
            /**
             * @brief Construct a new ValidationTool object
             * @param app Pointer to the Application instance
             */
            explicit ValidationTool(Application * app);

            /**
             * @brief Perform two-phase model validation
             * @param options JSON options for validation (compile flag, max_messages)
             * @return JSON response with validation results
             */
            nlohmann::json validateModel(const nlohmann::json & options = {});

            /**
             * @brief Validate model for manufacturing constraints
             * @param functionNames List of function names to validate
             * @param constraints Manufacturing constraints
             * @return JSON response with manufacturing validation results
             */
            nlohmann::json
            validateForManufacturing(const std::vector<std::string> & functionNames = {},
                                     const nlohmann::json & constraints = {});
        };
    } // namespace tools
} // namespace gladius
