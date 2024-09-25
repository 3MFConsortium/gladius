#include "EventLogger.h"

#include <iostream>

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

    void Logger::addEvent(Event const & event)
    {
        if (event.getSeverity() == Severity::Error || event.getSeverity() == Severity::FatalError)
        {
            m_countErrors++;
        }
        m_events.push_back(event);
#ifdef _DEBUG
        std::cout << event.getMessage() << "\n";
#endif
    }

    void Logger::clear()
    {
        m_events.clear();
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
}
