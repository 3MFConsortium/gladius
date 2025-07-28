/**
 * @file ApplicationMCPAdapter.cpp
 * @brief Implementation of Application MCP adapter
 */

#include "ApplicationMCPAdapter.h"
#include "../Application.h" // Only include here, not in header

namespace gladius
{
    ApplicationMCPAdapter::ApplicationMCPAdapter(Application * app)
        : m_application(app)
    {
    }

    std::string ApplicationMCPAdapter::getVersion() const
    {
        if (!m_application)
        {
            return "Unknown";
        }

        // TODO: Get actual version from Application
        return "1.0.0";
    }

    bool ApplicationMCPAdapter::isRunning() const
    {
        return m_application != nullptr;
    }

    std::string ApplicationMCPAdapter::getApplicationName() const
    {
        return "Gladius";
    }

    std::string ApplicationMCPAdapter::getStatus() const
    {
        if (!m_application)
        {
            return "not_running";
        }

        return "running";
    }

    bool ApplicationMCPAdapter::hasActiveDocument() const
    {
        if (!m_application)
        {
            return false;
        }

        // TODO: Check if Application has an active document
        return false;
    }

    std::string ApplicationMCPAdapter::getActiveDocumentPath() const
    {
        if (!m_application)
        {
            return "";
        }

        // TODO: Get actual document path from Application
        return "";
    }
}
