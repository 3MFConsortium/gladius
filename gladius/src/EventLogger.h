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
        virtual void addEvent(Event const & event);

        void clear();

        [[nodiscard]] Events::iterator begin();

        [[nodiscard]] Events::iterator end();

        [[nodiscard]] Events::const_iterator cbegin() const;

        [[nodiscard]] Events::const_iterator cend() const;

        [[nodiscard]] size_t size() const;

        [[nodiscard]] size_t getErrorCount() const;
      private:
        Events m_events;
        size_t m_countErrors{};
    };

    using SharedLogger = std::shared_ptr<Logger>;
}