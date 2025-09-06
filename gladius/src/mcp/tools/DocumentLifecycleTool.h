/**
 * @file DocumentLifecycleTool.h
 * @brief MCP tool for document lifecycle operations (create, open, save, export)
 */

#pragma once

#include "MCPToolBase.h"
#include <string>

namespace gladius::mcp::tools
{
    /**
     * @brief Tool for handling document lifecycle operations
     *
     * This tool encapsulates all document-related operations such as:
     * - Creating new documents
     * - Opening existing documents
     * - Saving documents
     * - Exporting documents to different formats
     *
     * @note SYNCHRONOUS OPERATIONS WARNING:
     * This tool uses synchronous operations that may block the calling thread,
     * particularly for file I/O operations. For async behavior in MCP contexts,
     * prefer using ApplicationMCPAdapter which delegates to CoroMCPAdapter.
     * This tool is primarily intended for direct tool usage, testing scenarios,
     * or cases where blocking behavior is acceptable.
     */
    class DocumentLifecycleTool : public MCPToolBase
    {
      public:
        explicit DocumentLifecycleTool(Application * app);

        /// @brief Create a new empty document
        bool createNewDocument();

        /// @brief Create a new empty document without template (for testing)
        bool createEmptyDocument();

        /// @brief Open a document from file path
        /// @param path Absolute path to the document file
        bool openDocument(const std::string & path);

        /// @brief Save the current document
        /// @note SYNCHRONOUS: This operation blocks until save completes
        /// @note For async behavior, use ApplicationMCPAdapter instead
        bool saveDocument();

        /// @brief Save the current document to a new path
        /// @param path Absolute path where to save the document
        /// @note SYNCHRONOUS: This operation blocks until save completes
        /// @note For async behavior, use ApplicationMCPAdapter instead
        bool saveDocumentAs(const std::string & path);

        /// @brief Export the current document to a different format
        /// @param path Absolute path for the exported file
        /// @param format Export format (e.g., "3mf", "stl", "obj")
        bool exportDocument(const std::string & path, const std::string & format);

        /// @brief Check if the current document has an active file path
        bool hasActiveDocument() const;

        /// @brief Get the path of the currently active document
        /// @return Path string, empty if no active document
        std::string getActiveDocumentPath() const;
    };
} // namespace gladius::mcp::tools
