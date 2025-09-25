/**
 * @file AsyncMCPToolBase.cpp
 * @brief Implementation of base class for async MCP tools
 */

#include "AsyncMCPToolBase.h"
#include "../../Application.h"
#include "../CoroMCPAdapter.h"

namespace gladius::mcp::tools
{
    AsyncMCPToolBase::AsyncMCPToolBase(Application * app)
        : MCPToolBase(app)
        , m_coroAdapter(nullptr)
    {
        // Initialize the coroutine adapter for async operations
        if (m_application)
        {
            m_coroAdapter = std::make_unique<mcp::CoroMCPAdapter>(m_application);
        }
    }
} // namespace gladius::mcp::tools
