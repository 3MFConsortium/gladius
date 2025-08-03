#pragma once

#include <chrono>
#include <coro/coro.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace gladius::events
{
    enum class Severity
    {
        Info,
        Warning,
        Error,
        FatalError
    };

    enum class OutputMode
    {
        Console, ///< Output to console (normal mode)
        Silent   ///< Silent mode (for MCP stdio transport)
    };

    class Event
    {
      public:
        Event(std::string msg, Severity severity = Severity::Warning);

        [[nodiscard]] std::chrono::system_clock::time_point getTimeStamp() const;

        [[nodiscard]] std::string getMessage() const;

        [[nodiscard]] Severity getSeverity() const;

      private:
        std::chrono::system_clock::time_point m_timestamp;
        std::string m_msg;
        Severity m_severity;
    };

    using Events = std::vector<Event>;
    class Logger
    {
      public:
        Logger() = default;
        explicit Logger(OutputMode mode)
            : m_outputMode(mode)
        {
        }

        /// Initialize the logger with file logging capability
        void initialize();

        /// Cleanup resources
        ~Logger();

        virtual void addEvent(Event const & event);

        void clear();

        /// Set output mode (Console or Silent)
        void setOutputMode(OutputMode mode)
        {
            m_outputMode = mode;
        }

        /// Get current output mode
        OutputMode getOutputMode() const
        {
            return m_outputMode;
        }

        /// Convenience methods for logging different severity levels
        void logInfo(const std::string & message);
        void logWarning(const std::string & message);
        void logError(const std::string & message);
        void logFatalError(const std::string & message);

        /// Get the log file path (if file logging is enabled)
        [[nodiscard]] std::filesystem::path getLogFilePath() const;

        /// Enable or disable file logging
        void setFileLoggingEnabled(bool enabled);

        /// Check if file logging is enabled
        [[nodiscard]] bool isFileLoggingEnabled() const
        {
            return m_fileLoggingEnabled;
        }

        /// Flush pending log entries to file
        coro::task<void> flushToFile();

        /// @brief Flush any pending file operations (blocks until complete)
        void flush();

        [[nodiscard]] Events::iterator begin();

        [[nodiscard]] Events::iterator end();

        [[nodiscard]] Events::const_iterator cbegin() const;

        [[nodiscard]] Events::const_iterator cend() const;

        [[nodiscard]] size_t size() const;

        [[nodiscard]] size_t getErrorCount() const;

        [[nodiscard]] size_t getWarningCount() const;

      private:
        /// Create the log directory if it doesn't exist
        void ensureLogDirectoryExists();

        /// Generate a unique log filename
        std::string generateLogFilename() const;

        /// Format an event for file output
        std::string formatEventForFile(Event const & event) const;

        /// Write queued events to file asynchronously
        coro::task<void> writeEventsToFile();

        /// Schedule an async write operation
        void scheduleAsyncWrite();

        /// Check if enough time has passed to warrant a flush
        bool shouldFlushByTime() const;

        Events m_events;
        size_t m_countErrors{};
        size_t m_countWarnings{};
        OutputMode m_outputMode{OutputMode::Console};

        /// File logging members
        bool m_fileLoggingEnabled{true};
        std::filesystem::path m_logDirectory;
        std::filesystem::path m_logFilePath;
        std::vector<Event> m_pendingFileEvents;
        bool m_initialized{false};
        std::shared_ptr<coro::thread_pool> m_fileWritePool;
        std::chrono::steady_clock::time_point m_lastFlushTime{std::chrono::steady_clock::now()};
        static constexpr std::chrono::seconds FLUSH_INTERVAL{1};
    };

    using SharedLogger = std::shared_ptr<Logger>;
}