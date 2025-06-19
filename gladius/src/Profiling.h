#pragma once

#include <chrono>
#include <iostream>
#include <thread>
#include <tracy/Tracy.hpp>
#include <chrono>
#include <string>

namespace gladius
{
    class ScopedProfilingFrame
    {
      public:
        ScopedProfilingFrame(const std::string & name)
            : m_name(name)
        {
            FrameMarkStart(m_name.c_str());
        }

        ~ScopedProfilingFrame()
        {
            FrameMarkEnd(m_name.c_str());
        }

      private:
        const std::string m_name;
    };

#define LOG_LOCATION
    //#define LOG_LOCATION std::cout << "Method: " << __FUNCTION__ << " line: " << __LINE__ << ",
    //Thread ID: " << std::this_thread::get_id() << std::endl; #define ProfileFunction ZoneScoped;

    class ScopedTimeLogger
    {
      public:
        ScopedTimeLogger(const std::string & name)
            : m_name(name)
        {
            m_start = std::chrono::high_resolution_clock::now();
        }

        ~ScopedTimeLogger()
        {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - m_start);
            constexpr int64_t threshold = 1;

            if (duration.count() > threshold)
            {
                std::cout << m_name << " took " << duration.count() << "ms" << std::endl;
            }
        }

      private:
        std::string m_name;
        std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
    };
#define LOG_SCOPE_DURATION ScopedTimeLogger scopedTimeLogger(__FUNCTION__);
#define LOG_SCOPE_DURATION_NAMED(name) ScopedTimeLogger scopedTimeLogger(name);
//#define ProfileFunction LOG_SCOPE_DURATION
//#define ProfileFunction ZoneScoped;
#define ProfileFunction 
}