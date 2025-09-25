#pragma once

#include <coro/coro.hpp>
#include <functional>
#include <memory>
#include <string>

// Forward declarations
namespace gladius
{
    class Document;
    class Application;
}

namespace gladius::mcp
{

    /**
     * @brief Coroutine-based MCP adapter that prevents UI blocking and resource conflicts
     *
     * This adapter provides asynchronous operations using libcoro to ensure that:
     * - Heavy operations (save, load, render) run on background threads
     * - OpenCL operations are properly isolated to prevent resource conflicts
     * - The UI thread remains responsive during long-running operations
     * - MCP requests can be processed without blocking the application
     */
    class CoroMCPAdapter
    {
      private:
        Application * m_application;
        std::shared_ptr<coro::thread_pool> m_backgroundPool;
        std::shared_ptr<coro::thread_pool> m_computePool;
        std::string m_lastErrorMessage;

      public:
        /**
         * @brief Construct a new CoroMCPAdapter
         *
         * @param application Pointer to the main application instance
         * @param backgroundThreads Number of threads for I/O operations (default: 2)
         * @param computeThreads Number of threads for OpenCL operations (default: 4)
         */
        explicit CoroMCPAdapter(Application * application,
                                std::size_t backgroundThreads = 2,
                                std::size_t computeThreads = 4);

        ~CoroMCPAdapter();

        /**
         * @brief Asynchronously save document to specified path
         *
         * This operation runs on a background thread to prevent UI blocking.
         * The document is saved without interfering with OpenCL operations.
         *
         * @param path File path to save the document to
         * @return coro::task<bool> Coroutine that returns true on success, false on failure
         */
        auto saveDocumentAsync(const std::string & path) -> coro::task<bool>;

        /**
         * @brief Asynchronously save document with thumbnail generation
         *
         * This operation:
         * 1. Saves the document on a background thread
         * 2. Generates thumbnail on a compute thread (OpenCL isolated)
         * 3. Updates UI if not in headless mode
         *
         * @param path File path to save the document to
         * @return coro::task<bool> Coroutine that returns true on success, false on failure
         */
        auto saveDocumentWithThumbnailAsync(const std::string & path) -> coro::task<bool>;

        /**
         * @brief Synchronous wrapper for saveDocumentAsync for MCP interface
         *
         * This provides the synchronous interface required by MCP while internally
         * using asynchronous coroutines to prevent blocking.
         *
         * @param path File path to save the document to
         * @return bool True on success, false on failure
         */
        bool saveDocumentAs(const std::string & path);

        /**
         * @brief Asynchronously generate thumbnail for the current document
         *
         * This operation runs on the compute thread pool to isolate OpenCL
         * operations and prevent resource conflicts.
         *
         * @return coro::task<bool> Coroutine that returns true on success, false on failure
         */
        auto generateThumbnailAsync() -> coro::task<bool>;

        /**
         * @brief Get the last error message from operations
         *
         * @return const std::string& Error message describing the last failure
         */
        const std::string & getLastErrorMessage() const
        {
            return m_lastErrorMessage;
        }

        /**
         * @brief Get the background thread pool for I/O operations
         *
         * @return std::shared_ptr<coro::thread_pool> Shared pointer to background pool
         */
        std::shared_ptr<coro::thread_pool> getBackgroundPool() const
        {
            return m_backgroundPool;
        }

        /**
         * @brief Get the compute thread pool for OpenCL operations
         *
         * @return std::shared_ptr<coro::thread_pool> Shared pointer to compute pool
         */
        std::shared_ptr<coro::thread_pool> getComputePool() const
        {
            return m_computePool;
        }

      private:
        /**
         * @brief Validate file path for document operations
         *
         * @param path File path to validate
         * @return bool True if path is valid, false otherwise
         */
        bool validatePath(const std::string & path);

        /**
         * @brief Set error message and return false for convenience
         *
         * @param message Error message to set
         * @return bool Always returns false
         */
        bool setError(const std::string & message);
    };

} // namespace gladius::mcp
