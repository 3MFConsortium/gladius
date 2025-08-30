/**
 * @file MCPToolBase.cpp
 * @brief Implementation of base class for MCP tool implementations
 */

#include "MCPToolBase.h"
#include "../../Application.h"
#include "../../Document.h"

namespace gladius::mcp::tools
{
    MCPToolBase::MCPToolBase(Application * app)
        : m_application(app)
        , m_lastErrorMessage()
    {
    }

    bool MCPToolBase::validateApplication() const
    {
        if (!m_application)
        {
            setErrorMessage("Application instance is null");
            return false;
        }
        return true;
    }

    bool MCPToolBase::validateActiveDocument() const
    {
        if (!validateApplication())
        {
            return false;
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            setErrorMessage("No active document available");
            return false;
        }
        return true;
    }

    void MCPToolBase::setErrorMessage(const std::string & message) const
    {
        m_lastErrorMessage = message;
    }
} // namespace gladius::mcp::tools
