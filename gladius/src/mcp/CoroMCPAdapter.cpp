#include "CoroMCPAdapter.h"
#include "../Application.h"
#include "../Document.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

namespace gladius::mcp
{

    CoroMCPAdapter::CoroMCPAdapter(Application * application,
                                   std::size_t backgroundThreads,
                                   std::size_t computeThreads)
        : m_application(application)
        , m_lastErrorMessage("No error")
    {
        if (!m_application)
        {
            throw std::invalid_argument("Application pointer cannot be null");
        }

        // Create thread pools for different operation types
        m_backgroundPool = coro::thread_pool::make_shared(coro::thread_pool::options{
          .thread_count = static_cast<uint32_t>(backgroundThreads),
          .on_thread_start_functor = [](std::size_t worker_idx) -> void
          { std::cout << "MCP background worker " << worker_idx << " starting\n"; },
          .on_thread_stop_functor = [](std::size_t worker_idx) -> void
          { std::cout << "MCP background worker " << worker_idx << " stopping\n"; }});

        m_computePool = coro::thread_pool::make_shared(coro::thread_pool::options{
          .thread_count = static_cast<uint32_t>(computeThreads),
          .on_thread_start_functor = [](std::size_t worker_idx) -> void
          { std::cout << "MCP compute worker " << worker_idx << " starting\n"; },
          .on_thread_stop_functor = [](std::size_t worker_idx) -> void
          { std::cout << "MCP compute worker " << worker_idx << " stopping\n"; }});
    }

    CoroMCPAdapter::~CoroMCPAdapter()
    {
        // Shutdown thread pools gracefully
        if (m_backgroundPool)
        {
            m_backgroundPool->shutdown();
        }
        if (m_computePool)
        {
            m_computePool->shutdown();
        }
    }

    auto CoroMCPAdapter::saveDocumentAsync(const std::string & path) -> coro::task<bool>
    {
        // Validate input on current thread
        if (!validatePath(path))
        {
            co_return false;
        }

        // Get document reference (main thread operation)
        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            setError("No active document available");
            co_return false;
        }

        try
        {
            // Switch to background thread for I/O operations
            co_await m_backgroundPool->schedule();

            // This now runs on background thread - no UI blocking!
            document->saveAs(std::filesystem::path(path));

            // Success
            m_lastErrorMessage = "Document saved successfully";
            co_return true;
        }
        catch (const std::exception & e)
        {
            setError("Save operation failed: " + std::string(e.what()));
            co_return false;
        }
    }

    auto CoroMCPAdapter::saveDocumentWithThumbnailAsync(const std::string & path)
      -> coro::task<bool>
    {
        // Validate input
        if (!validatePath(path))
        {
            co_return false;
        }

        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            setError("No active document available");
            co_return false;
        }

        try
        {
            // Start save and thumbnail operations in parallel
            auto saveTask = [this, &path, document]() -> coro::task<bool>
            {
                co_await m_backgroundPool->schedule();
                document->saveAs(std::filesystem::path(path));
                co_return true;
            };

            auto thumbnailTask = [this, document]() -> coro::task<bool>
            {
                co_await m_computePool->schedule();

                // TODO: Implement proper OpenCL thumbnail generation
                // This would use our existing thumbnail generation code
                // but isolated to the compute thread pool

                // For now, just simulate the operation
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                co_return true;
            };

            // Create the tasks
            auto saveOperation = saveTask();
            auto thumbnailOperation = thumbnailTask();

            // Wait for both operations to complete
            auto [saveTaskResult, thumbnailTaskResult] =
              co_await coro::when_all(std::move(saveOperation), std::move(thumbnailOperation));

            if (!saveTaskResult.return_value())
            {
                setError("Document save failed");
                co_return false;
            }

            if (!thumbnailTaskResult.return_value())
            {
                setError("Thumbnail generation failed");
                // Don't fail the save operation just because thumbnail failed
            }

            m_lastErrorMessage = "Document saved successfully with thumbnail";
            co_return true;
        }
        catch (const std::exception & e)
        {
            setError("Save with thumbnail failed: " + std::string(e.what()));
            co_return false;
        }
    }

    bool CoroMCPAdapter::saveDocumentAs(const std::string & path)
    {
        try
        {
            // Use coro::sync_wait to bridge async/sync worlds
            // This blocks the current thread but the actual work happens on background threads
            return coro::sync_wait(saveDocumentWithThumbnailAsync(path));
        }
        catch (const std::exception & e)
        {
            setError("Synchronous save wrapper failed: " + std::string(e.what()));
            return false;
        }
    }

    auto CoroMCPAdapter::generateThumbnailAsync() -> coro::task<bool>
    {
        auto document = m_application->getCurrentDocument();
        if (!document)
        {
            setError("No active document available for thumbnail generation");
            co_return false;
        }

        try
        {
            // Switch to compute thread for OpenCL operations
            co_await m_computePool->schedule();

            // TODO: Implement actual thumbnail generation
            // This would call into our existing OpenCL rendering code
            // but safely isolated on the compute thread

            // For now, simulate the operation
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            m_lastErrorMessage = "Thumbnail generated successfully";
            co_return true;
        }
        catch (const std::exception & e)
        {
            setError("Thumbnail generation failed: " + std::string(e.what()));
            co_return false;
        }
    }

    bool CoroMCPAdapter::validatePath(const std::string & path)
    {
        if (path.empty())
        {
            setError("File path cannot be empty");
            return false;
        }

        if (!path.ends_with(".3mf"))
        {
            setError("File must have .3mf extension");
            return false;
        }

        // Check if directory exists and is writable
        std::filesystem::path filePath(path);
        auto parentDir = filePath.parent_path();

        if (!parentDir.empty() && !std::filesystem::exists(parentDir))
        {
            try
            {
                std::filesystem::create_directories(parentDir);
            }
            catch (const std::exception & e)
            {
                setError("Cannot create directory: " + std::string(e.what()));
                return false;
            }
        }

        return true;
    }

    bool CoroMCPAdapter::setError(const std::string & message)
    {
        m_lastErrorMessage = message;
        std::cerr << "MCP Error: " << message << std::endl;
        return false;
    }

} // namespace gladius::mcp
