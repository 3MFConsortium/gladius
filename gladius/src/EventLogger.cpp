#include "EventLogger.h"

#include <coro/coro.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace gladius::events
{
    Event::Event(std::string msg, Severity severity)
        : m_timestamp(std::chrono::system_clock::now())
        , m_msg(std::move(msg))
        , m_severity(severity)
    {
    }

    std::chrono::system_clock::time_point Event::getTimeStamp() const
    {
        return m_timestamp;
    }

    std::string Event::getMessage() const
    {
        return m_msg;
    }

    Severity Event::getSeverity() const
    {
        return m_severity;
    }

    void Logger::initialize()
    {
        if (m_initialized)
        {
            return;
        }

        // Initialize log directory
        m_logDirectory = std::filesystem::temp_directory_path() / "gladius" / "logs";
        ensureLogDirectoryExists();

        // Generate unique log filename
        m_logFilePath = m_logDirectory / generateLogFilename();

        // Initialize thread pool for file operations
        if (m_fileLoggingEnabled)
        {
            m_fileWritePool = coro::thread_pool::make_shared(coro::thread_pool::options{
              .thread_count = 1, // Single thread for file writing to avoid conflicts
              .on_thread_start_functor = [](std::size_t) {},
              .on_thread_stop_functor = [](std::size_t) {}});
        }

        m_initialized = true;

        // Write initial log entry
        logInfo("Logger initialized - file logging enabled");
    }

    Logger::~Logger()
    {
        // Ensure all pending events are written before destruction
        if (m_fileLoggingEnabled && m_initialized)
        {
            try
            {
                // First, try to flush using the async mechanism if available
                if (m_fileWritePool && !m_pendingFileEvents.empty())
                {
                    // Use sync_wait to ensure async write completes before destruction
                    coro::sync_wait(writeEventsToFile());
                }
                else if (!m_pendingFileEvents.empty())
                {
                    // Fallback: synchronous flush on destruction to ensure logs are written
                    std::ofstream logFile(m_logFilePath, std::ios::app);
                    if (logFile.is_open())
                    {
                        for (Event const & event : m_pendingFileEvents)
                        {
                            logFile << formatEventForFile(event) << std::endl;
                        }
                        logFile.flush();
                    }
                }
            }
            catch (...)
            {
                // Ignore errors during destruction, but try synchronous fallback
                try
                {
                    if (!m_pendingFileEvents.empty())
                    {
                        std::ofstream logFile(m_logFilePath, std::ios::app);
                        if (logFile.is_open())
                        {
                            for (Event const & event : m_pendingFileEvents)
                            {
                                logFile << formatEventForFile(event) << std::endl;
                            }
                            logFile.flush();
                        }
                    }
                }
                catch (...)
                {
                    // Final fallback failed, but we can't do more during destruction
                }
            }
        }

        // Shutdown thread pool after ensuring all pending writes are complete
        if (m_fileWritePool)
        {
            m_fileWritePool->shutdown();
        }
    }

    void Logger::addEvent(Event const & event)
    {
        // Initialize on first use if file logging is enabled but not yet initialized
        if (m_fileLoggingEnabled && !m_initialized)
        {
            initialize();
        }

        if (event.getSeverity() == Severity::Error || event.getSeverity() == Severity::FatalError)
        {
            m_countErrors++;
        }
        else if (event.getSeverity() == Severity::Warning)
        {
            m_countWarnings++;
        }
        m_events.push_back(event);

        // Handle file logging
        if (m_fileLoggingEnabled && m_initialized)
        {
            if (event.getSeverity() == Severity::FatalError)
            {
                // Fatal errors: write immediately (synchronous)
                try
                {
                    std::ofstream logFile(m_logFilePath, std::ios::app);
                    if (logFile.is_open())
                    {
                        logFile << formatEventForFile(event) << std::endl;
                        logFile.flush();

                        // Update last flush time after immediate write
                        m_lastFlushTime = std::chrono::steady_clock::now();
                    }
                }
                catch (...)
                {
                    // If writing fails, disable file logging
                    m_fileLoggingEnabled = false;
                }
            }
            else
            {
                // Other events: add to batch for async writing
                m_pendingFileEvents.push_back(event);

                // Trigger async write if we have enough events, if this is an error, or if enough
                // time has passed
                if (m_pendingFileEvents.size() >= 10 || event.getSeverity() == Severity::Error ||
                    shouldFlushByTime())
                {
                    scheduleAsyncWrite();
                }
            }
        }

        // Only output to console if not in silent mode
        if (m_outputMode == OutputMode::Console)
        {
            // In release mode, output to stderr to keep stdout clean for MCP
            std::cerr << event.getMessage() << "\n";
        }
    }

    void Logger::logInfo(const std::string & message)
    {
        addEvent(Event(message, Severity::Info));
    }

    void Logger::logWarning(const std::string & message)
    {
        addEvent(Event(message, Severity::Warning));
    }

    void Logger::logError(const std::string & message)
    {
        addEvent(Event(message, Severity::Error));
    }

    void Logger::logFatalError(const std::string & message)
    {
        addEvent(Event(message, Severity::FatalError));
    }

    void Logger::clear()
    {
        flush();
        m_events.clear();
        m_countErrors = 0;
        m_countWarnings = 0;
        m_pendingFileEvents.clear();
    }

    void Logger::flush()
    {
        if (m_fileLoggingEnabled && !m_pendingFileEvents.empty())
        {
            try
            {
                if (m_fileWritePool)
                {
                    // Use sync_wait to block until async write completes
                    coro::sync_wait(writeEventsToFile());
                }
                else
                {
                    // Fallback to synchronous write if no thread pool
                    (void) coro::sync_wait(flushToFile());
                }
            }
            catch (std::exception const &)
            {
                // If async flush fails, try synchronous fallback
                (void) coro::sync_wait(flushToFile());
            }
        }
    }

    Events::iterator Logger::begin()
    {
        return m_events.begin();
    }

    Events::iterator Logger::end()
    {
        return m_events.end();
    }

    Events::const_iterator Logger::cbegin() const
    {
        return m_events.cbegin();
    }

    Events::const_iterator Logger::cend() const
    {
        return m_events.cend();
    }

    size_t Logger::size() const
    {
        return m_events.size();
    }

    size_t Logger::getErrorCount() const
    {
        return m_countErrors;
    }

    size_t Logger::getWarningCount() const
    {
        return m_countWarnings;
    }

    std::filesystem::path Logger::getLogFilePath() const
    {
        return m_logFilePath;
    }

    void Logger::setFileLoggingEnabled(bool enabled)
    {
        m_fileLoggingEnabled = enabled;
        if (enabled && !m_initialized)
        {
            initialize();
        }
    }

    coro::task<void> Logger::flushToFile()
    {
        if (m_pendingFileEvents.empty())
        {
            co_return;
        }

        try
        {
            // Create a copy of pending events and clear the original
            std::vector<Event> eventsToWrite;
            eventsToWrite.swap(m_pendingFileEvents);

            // Write to file synchronously
            std::ofstream logFile(m_logFilePath, std::ios::app);
            if (logFile.is_open())
            {
                for (Event const & event : eventsToWrite)
                {
                    logFile << formatEventForFile(event) << std::endl;
                }
                logFile.flush();

                // Update last flush time after successful write
                m_lastFlushTime = std::chrono::steady_clock::now();
            }
            else
            {
                // If we can't open the file, put events back and disable file logging
                m_pendingFileEvents = std::move(eventsToWrite);
                m_fileLoggingEnabled = false;
            }
        }
        catch (std::exception const &)
        {
            // If writing fails, disable file logging to prevent continuous errors
            m_fileLoggingEnabled = false;
        }
    }

    void Logger::ensureLogDirectoryExists()
    {
        try
        {
            if (!std::filesystem::exists(m_logDirectory))
            {
                std::filesystem::create_directories(m_logDirectory);
            }
        }
        catch (std::exception const &)
        {
            // Directory creation failure will be handled when trying to write
            m_fileLoggingEnabled = false;
        }
    }

    std::string Logger::generateLogFilename() const
    {
        auto now = std::chrono::system_clock::now();
        auto time_t_val = std::chrono::system_clock::to_time_t(now);

        std::tm tm_result;
#ifdef _WIN32
        localtime_s(&tm_result, &time_t_val);
#else
        localtime_r(&time_t_val, &tm_result);
#endif

        std::ostringstream oss;
        oss << "gladius_" << std::put_time(&tm_result, "%Y%m%d_%H%M%S") << ".log";
        return oss.str();
    }

    std::string Logger::formatEventForFile(Event const & event) const
    {
        auto time_t_val = std::chrono::system_clock::to_time_t(event.getTimeStamp());

        std::tm tm_result;
#ifdef _WIN32
        localtime_s(&tm_result, &time_t_val);
#else
        localtime_r(&time_t_val, &tm_result);
#endif

        std::ostringstream oss;
        oss << "[" << std::put_time(&tm_result, "%Y-%m-%d %H:%M:%S") << "] ";

        // Add severity prefix
        switch (event.getSeverity())
        {
        case Severity::Info:
            oss << "[INFO] ";
            break;
        case Severity::Warning:
            oss << "[WARN] ";
            break;
        case Severity::Error:
            oss << "[ERROR] ";
            break;
        case Severity::FatalError:
            oss << "[FATAL] ";
            break;
        }

        oss << event.getMessage();
        return oss.str();
    }

    coro::task<void> Logger::writeEventsToFile()
    {
        if (m_pendingFileEvents.empty())
        {
            co_return;
        }

        try
        {
            // Switch to the file writing thread pool
            co_await m_fileWritePool->schedule();

            // Create a copy of pending events to avoid issues with concurrent access
            std::vector<Event> eventsToWrite;
            eventsToWrite.swap(m_pendingFileEvents);

            // Write to file
            std::ofstream logFile(m_logFilePath, std::ios::app);
            if (logFile.is_open())
            {
                for (Event const & event : eventsToWrite)
                {
                    logFile << formatEventForFile(event) << std::endl;
                }
                logFile.flush();

                // Update last flush time after successful write
                m_lastFlushTime = std::chrono::steady_clock::now();
            }
        }
        catch (std::exception const &)
        {
            // If writing fails, disable file logging to prevent continuous errors
            m_fileLoggingEnabled = false;
        }
    }

    void Logger::scheduleAsyncWrite()
    {
        if (!m_fileWritePool || m_pendingFileEvents.empty())
        {
            return;
        }

        // Create a detached coroutine that will run asynchronously
        auto task = [this]() -> coro::task<void>
        {
            try
            {
                co_await writeEventsToFile();
            }
            catch (...)
            {
                // Log write failed, but we don't propagate the error
                // since this is a fire-and-forget operation
            }
        }();

        // Explicitly discard the return value to silence the warning
        (void) task;
    }

    bool Logger::shouldFlushByTime() const
    {
        auto now = std::chrono::steady_clock::now();
        return (now - m_lastFlushTime) >= FLUSH_INTERVAL;
    }
}
