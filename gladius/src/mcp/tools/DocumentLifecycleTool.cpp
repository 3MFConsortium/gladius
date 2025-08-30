/**
 * @file DocumentLifecycleTool.cpp
 * @brief Implementation of MCP tool for document lifecycle operations
 */

#include "DocumentLifecycleTool.h"
#include "../../Application.h"
#include "../../Document.h"
#include <filesystem>

namespace gladius::mcp::tools
{
    DocumentLifecycleTool::DocumentLifecycleTool(Application * app)
        : MCPToolBase(app)
    {
    }

    bool DocumentLifecycleTool::createNewDocument()
    {
        if (!validateApplication())
        {
            return false;
        }

        try
        {
            if (m_application->isHeadlessMode())
            {
                // Ensure headless has a valid core/document if UI wasn't started
                if (!m_application->getMainWindow().getCurrentDocument())
                {
                    m_application->getMainWindow().setupHeadless(m_application->getGlobalLogger());
                }

                // In headless mode avoid UI-only code paths; create from template directly.
                auto doc = m_application->getCurrentDocument();
                if (!doc)
                {
                    return false;
                }
                doc->newFromTemplate();
                // Welcome screen is a UI concept but safe to hide regardless
                m_application->getMainWindow().hideWelcomeScreen();
                return true;
            }

            // UI mode: use the full UI flow
            m_application->getMainWindow().newModel();
            m_application->getMainWindow().hideWelcomeScreen();
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool DocumentLifecycleTool::openDocument(const std::string & path)
    {
        if (!validateApplication())
        {
            return false;
        }

        try
        {
            // Ensure headless has a valid core/document if UI wasn't started
            if (m_application->isHeadlessMode() &&
                !m_application->getMainWindow().getCurrentDocument())
            {
                m_application->getMainWindow().setupHeadless(m_application->getGlobalLogger());
            }
            // Use MainWindow's open method to properly hide welcome screen and handle UI updates
            m_application->getMainWindow().open(std::filesystem::path(path));
            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
    }

    bool DocumentLifecycleTool::saveDocument()
    {
        if (!validateApplication())
        {
            setErrorMessage("No application instance available");
            return false;
        }

        // NOTE: This is a synchronous implementation that directly calls Document::saveAs
        // The ApplicationMCPAdapter should use CoroMCPAdapter for async operations instead.
        // This tool method is primarily for direct tool usage or testing scenarios
        // where async behavior is not required.
        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                setErrorMessage(
                  "No active document available. Please create or open a document first.");
                return false;
            }

            // Check if document has a current filename
            auto currentPath = document->getCurrentAssemblyFilename();
            if (!currentPath.has_value())
            {
                setErrorMessage("Document has not been saved before. Use 'save_document_as' to "
                                "specify a filename.");
                return false;
            }

            // SYNCHRONOUS OPERATION: This will block until the save completes
            // For async behavior, use ApplicationMCPAdapter which delegates to CoroMCPAdapter
            document->saveAs(currentPath.value());
            return true;
        }
        catch (const std::exception & e)
        {
            setErrorMessage("Exception while saving document: " + std::string(e.what()));
            return false;
        }
    }

    bool DocumentLifecycleTool::saveDocumentAs(const std::string & path)
    {
        if (!validateApplication())
        {
            setErrorMessage("No application instance available");
            return false;
        }

        if (path.empty())
        {
            setErrorMessage("File path cannot be empty");
            return false;
        }

        // NOTE: This is a synchronous implementation that directly calls Document::saveAs
        // The ApplicationMCPAdapter should use CoroMCPAdapter for async operations instead.
        // This tool method is primarily for direct tool usage or testing scenarios
        // where async behavior is not required.
        try
        {
            auto document = m_application->getCurrentDocument();
            if (!document)
            {
                setErrorMessage(
                  "No active document available. Please create or open a document first.");
                return false;
            }

            // Convert string to filesystem::path for Document::saveAs
            std::filesystem::path filePath(path);

            // SYNCHRONOUS OPERATION: This will block until the save completes
            // For async behavior, use ApplicationMCPAdapter which delegates to CoroMCPAdapter
            document->saveAs(filePath);
            return true;
        }
        catch (const std::exception & e)
        {
            setErrorMessage("Exception while saving document to '" + path +
                            "': " + std::string(e.what()));
            return false;
        }
    }

    bool DocumentLifecycleTool::exportDocument(const std::string & path, const std::string & format)
    {
        if (!validateApplication())
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

    bool DocumentLifecycleTool::hasActiveDocument() const
    {
        if (!validateApplication())
        {
            return false;
        }

        auto document = m_application->getCurrentDocument();
        return document != nullptr;
    }

    std::string DocumentLifecycleTool::getActiveDocumentPath() const
    {
        if (!validateApplication())
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
} // namespace gladius::mcp::tools
