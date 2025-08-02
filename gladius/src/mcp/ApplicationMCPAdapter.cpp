/**
 * @file ApplicationMCPAdapter.cpp
 * @brief Implementation of Application MCP adapter
 */

#include "ApplicationMCPAdapter.h"
#include "../Application.h"
#include "../Document.h"
#include <filesystem> // Only include here, not in header
#include <nlohmann/json.hpp>

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

        auto document = m_application->getCurrentDocument();
        return document != nullptr;
    }

    std::string ApplicationMCPAdapter::getActiveDocumentPath() const
    {
        if (!m_application)
        {
            return "";
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            return "";
        }

        // Get the current assembly filename from the document
        auto filename = document->getCurrentAssemblyFilename();
        if (filename.has_value())
        {
            return filename->string();
        }

        return "";
    }

    bool ApplicationMCPAdapter::createNewDocument()
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            // Use MainWindow's newModel method and hide welcome screen like the UI callback does
            m_application->getMainWindow().newModel();
            m_application->getMainWindow().hideWelcomeScreen();
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool ApplicationMCPAdapter::openDocument(const std::string & path)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            // Use MainWindow's open method to properly hide welcome screen and handle UI updates
            m_application->getMainWindow().open(std::filesystem::path(path));
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool ApplicationMCPAdapter::saveDocument()
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                // This would require knowing the current filename
                // For now, return false until we implement proper saving
                return false; // TODO: Implement when Document has getCurrentFilename
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool ApplicationMCPAdapter::saveDocumentAs(const std::string & path)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                document->saveAs(std::filesystem::path(path));
                return true;
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool ApplicationMCPAdapter::exportDocument(const std::string & path, const std::string & format)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                if (format == "stl")
                {
                    document->exportAsStl(std::filesystem::path(path));
                    return true;
                }
                // Add more export formats as needed
                return false;
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool ApplicationMCPAdapter::setFloatParameter(uint32_t modelId,
                                                  const std::string & nodeName,
                                                  const std::string & parameterName,
                                                  float value)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                document->setFloatParameter(modelId, nodeName, parameterName, value);
                return true;
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    float ApplicationMCPAdapter::getFloatParameter(uint32_t modelId,
                                                   const std::string & nodeName,
                                                   const std::string & parameterName)
    {
        if (!m_application)
        {
            return 0.0f;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                return document->getFloatParameter(modelId, nodeName, parameterName);
            }
            return 0.0f;
        }
        catch (const std::exception &)
        {
            return 0.0f;
        }
    }

    bool ApplicationMCPAdapter::setStringParameter(uint32_t modelId,
                                                   const std::string & nodeName,
                                                   const std::string & parameterName,
                                                   const std::string & value)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                document->setStringParameter(modelId, nodeName, parameterName, value);
                return true;
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    std::string ApplicationMCPAdapter::getStringParameter(uint32_t modelId,
                                                          const std::string & nodeName,
                                                          const std::string & parameterName)
    {
        if (!m_application)
        {
            return "";
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                return document->getStringParameter(modelId, nodeName, parameterName);
            }
            return "";
        }
        catch (const std::exception &)
        {
            return "";
        }
    }

    bool ApplicationMCPAdapter::createFunctionFromExpression(const std::string & name,
                                                             const std::string & expression,
                                                             const std::string & outputType)
    {
        if (!m_application)
        {
            return false;
        }

        try
        {
            auto document = m_application->getCurrentDocument();
            if (document)
            {
                // TODO: Implement expression-to-function conversion
                // This would use ExpressionParser and ExpressionToGraphConverter
                // to create a new function from the mathematical expression
                //
                // Examples of expressions that could be supported:
                // - TPMS structures: "sin(x*2*pi/10)*cos(y*2*pi/10) + sin(y*2*pi/10)*cos(z*2*pi/10)
                // + sin(z*2*pi/10)*cos(x*2*pi/10) - 0.2" (gyroid)
                // - Basic shapes: "sqrt(x*x + y*y + z*z) - 5" (sphere)
                // - Complex geometry: "max(abs(x) - 5, max(abs(y) - 3, abs(z) - 2))" (rounded box)
                // - Lattices: "min(sin(x), min(sin(y), sin(z)))" (simple lattice)

                // For now, return false until implementation is complete
                return false;
            }
            return false;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }
}
