#pragma once

#include "MCPToolBase.h"
#include <cstdint>
#include <string>

namespace gladius::mcp::tools
{
    /// @brief Tool for managing function parameters (float and string) in document models
    class ParameterManagementTool : public MCPToolBase
    {
      public:
        /// @brief Constructor - initializes tool with application reference
        /// @param application Pointer to the main application instance
        ParameterManagementTool(gladius::Application * application);

        /// @brief Destructor
        ~ParameterManagementTool() = default;

        /// @brief Set a float parameter value in a model node
        /// @param modelId The ID of the model containing the parameter
        /// @param nodeName The name of the node containing the parameter
        /// @param parameterName The name of the parameter to set
        /// @param value The float value to set
        /// @return true if parameter was set successfully, false otherwise
        bool setFloatParameter(uint32_t modelId,
                               const std::string & nodeName,
                               const std::string & parameterName,
                               float value);

        /// @brief Get a float parameter value from a model node
        /// @param modelId The ID of the model containing the parameter
        /// @param nodeName The name of the node containing the parameter
        /// @param parameterName The name of the parameter to get
        /// @return The float value of the parameter, or 0.0f if not found/error
        float getFloatParameter(uint32_t modelId,
                                const std::string & nodeName,
                                const std::string & parameterName);

        /// @brief Set a string parameter value in a model node
        /// @param modelId The ID of the model containing the parameter
        /// @param nodeName The name of the node containing the parameter
        /// @param parameterName The name of the parameter to set
        /// @param value The string value to set
        /// @return true if parameter was set successfully, false otherwise
        bool setStringParameter(uint32_t modelId,
                                const std::string & nodeName,
                                const std::string & parameterName,
                                const std::string & value);

        /// @brief Get a string parameter value from a model node
        /// @param modelId The ID of the model containing the parameter
        /// @param nodeName The name of the node containing the parameter
        /// @param parameterName The name of the parameter to get
        /// @return The string value of the parameter, or empty string if not found/error
        std::string getStringParameter(uint32_t modelId,
                                       const std::string & nodeName,
                                       const std::string & parameterName);
    };
} // namespace gladius::mcp::tools
