#pragma once

#include <chrono>
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

        [[nodiscard]] Events::iterator begin();

        [[nodiscard]] Events::iterator end();

        [[nodiscard]] Events::const_iterator cbegin() const;

        [[nodiscard]] Events::const_iterator cend() const;

        [[nodiscard]] size_t size() const;

        [[nodiscard]] size_t getErrorCount() const;

        [[nodiscard]] size_t getWarningCount() const;

      private:
        Events m_events;
        size_t m_countErrors{};
        size_t m_countWarnings{};
        OutputMode m_outputMode{OutputMode::Console};
    };

    using SharedLogger = std::shared_ptr<Logger>;
}