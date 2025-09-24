/**
 * @file MCPToolBase.h
 * @brief Base class for MCP tool implementations
 */

#pragma once

#include <string>

namespace gladius
{
    class Application; // Forward declaration

    namespace mcp
    {
        namespace tools
        {
            /**
             * @brief Abstract base class for MCP tool implementations
             *
             * Provides common functionality shared across all MCP tools including
             * Application reference management, error handling, and validation helpers.
             */
            class MCPToolBase
            {
              protected:
                Application * m_application; ///< Raw pointer to avoid circular dependencies
                mutable std::string m_lastErrorMessage; ///< Store detailed error information

                /// Common validation helpers
                bool validateApplication() const;
                bool validateActiveDocument() const;
                void setErrorMessage(const std::string & message) const;

              public:
                /**
                 * @brief Construct a new MCPToolBase object
                 * @param app Pointer to the Application instance
                 */
                explicit MCPToolBase(Application * app);

                /**
                 * @brief Virtual destructor for proper inheritance
                 */
                virtual ~MCPToolBase() = default;

                /**
                 * @brief Get the last error message for debugging
                 * @return std::string The last error message
                 */
                std::string getLastErrorMessage() const
                {
                    return m_lastErrorMessage;
                }
            };
        } // namespace tools
    } // namespace mcp
} // namespace gladius
