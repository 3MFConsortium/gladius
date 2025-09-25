/**
 * @file AsyncMCPToolBase.h
 * @brief Base class for MCP tools that require async operations
 */

#pragma once

#include "MCPToolBase.h"
#include <memory>

namespace gladius
{
    namespace mcp
    {
        class CoroMCPAdapter; // Forward declaration

        namespace tools
        {
            /**
             * @brief Base class for MCP tools that require async operations
             *
             * Extends MCPToolBase with coroutine adapter support for operations
             * that need to be performed asynchronously.
             */
            class AsyncMCPToolBase : public MCPToolBase
            {
              protected:
                std::unique_ptr<mcp::CoroMCPAdapter>
                  m_coroAdapter; ///< Coroutine adapter for async ops

              public:
                /**
                 * @brief Construct a new AsyncMCPToolBase object
                 * @param app Pointer to the Application instance
                 */
                explicit AsyncMCPToolBase(Application * app);

                /**
                 * @brief Virtual destructor for proper inheritance
                 */
                virtual ~AsyncMCPToolBase() = default;
            };
        } // namespace tools
    } // namespace mcp
} // namespace gladius
