/**
 * @file ApplicationLifecycleTool.cpp
 * @brief Implementation of MCP tool for application lifecycle operations
 */

#include "ApplicationLifecycleTool.h"
#include "../../Application.h"

namespace gladius::mcp::tools
{
    ApplicationLifecycleTool::ApplicationLifecycleTool(Application * app)
        : MCPToolBase(app)
    {
    }

    std::string ApplicationLifecycleTool::getVersion() const
    {
        if (!validateApplication())
        {
            return "Unknown";
        }

        // TODO: Get actual version from Application
        return "1.0.0";
    }

    bool ApplicationLifecycleTool::isRunning() const
    {
        return m_application != nullptr;
    }

    std::string ApplicationLifecycleTool::getApplicationName() const
    {
        return "Gladius";
    }

    std::string ApplicationLifecycleTool::getStatus() const
    {
        if (!validateApplication())
        {
            return "not_running";
        }

        return "running";
    }

    void ApplicationLifecycleTool::setHeadlessMode(bool headless)
    {
        if (validateApplication())
        {
            m_application->setHeadlessMode(headless);
        }
    }

    bool ApplicationLifecycleTool::isHeadlessMode() const
    {
        return m_application ? m_application->isHeadlessMode() : true;
    }

    bool ApplicationLifecycleTool::showUI()
    {
        return m_application ? m_application->showUI() : false;
    }

    bool ApplicationLifecycleTool::isUIRunning() const
    {
        return m_application ? m_application->isUIRunning() : false;
    }
} // namespace gladius::mcp::tools
