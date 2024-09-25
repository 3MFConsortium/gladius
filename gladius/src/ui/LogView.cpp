#include "LogView.h"

#include <fmt/chrono.h>
#include <time.h>

#include "IconsFontAwesome4.h"
#include "wordwarp.h"

namespace gladius::ui
{
    void LogView::show()
    {
        m_visible = true;
    }

    void LogView::hide()
    {
        m_visible = false;
    }

    void LogView::render(gladius::events::Logger & logger)
    {
        if (!m_visible)
        {
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Events", &m_visible);

        ImGui::Checkbox("Auto-scroll", &m_autoScroll);
        ImGui::SameLine();

        if (m_filter.Draw("Filter") || (m_logSizeWhenCacheWasGenerated != logger.size()))
        {
            if (m_filter.IsActive())
            {
                updateCache(logger);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear log"))
        {
            logger.clear();
        }

        auto const & eventsBegin =
          m_filter.IsActive() ? m_filteredEvents.cbegin() : logger.cbegin();
        auto const & eventsEnd = m_filter.IsActive() ? m_filteredEvents.cend() : logger.cend();

        ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

        auto const logSize = std::distance(eventsBegin, eventsEnd);
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(logSize));

        auto lastFatalErrorIter = logger.cend();
        while (clipper.Step())
        {
            auto displayBegin = eventsBegin + clipper.DisplayStart;
            auto displayEnd = eventsBegin + clipper.DisplayEnd;

            for (auto iter = displayBegin; iter != eventsEnd && iter != displayEnd; ++iter)
            {
                auto inTime = std::chrono::system_clock::to_time_t(iter->getTimeStamp());
              
                std::tm tm{};
#ifdef _MSVC_LANG
                localtime_s(&tm, &inTime);
#else
                localtime_r(&inTime, &tm);
#endif
                ImGui::TextUnformatted(fmt::format("{:%Y-%m-%d %H:%M:%S}", tm).c_str());
                ImGui::SameLine();

                switch (iter->getSeverity())
                {
                case events::Severity::Info:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.2f, 1.f, 1.0f));
                    ImGui::Text(reinterpret_cast<const char*>("\tINFO\t" ICON_FA_INFO));
                    ImGui::PopStyleColor();
                    break;
                case events::Severity::Warning:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.f, 1.0f));
                    ImGui::Text(reinterpret_cast<const char*>("\tWARNING\t" ICON_FA_EXCLAMATION_CIRCLE));
                    ImGui::PopStyleColor();
                    break;
                case events::Severity::Error:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                    ImGui::Text(reinterpret_cast<const char*>("\tERROR\t" ICON_FA_EXCLAMATION_TRIANGLE));
                    ImGui::PopStyleColor();
                    break;
                case events::Severity::FatalError:
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.f, 1.0f));
                    ImGui::Text(reinterpret_cast<const char*>("\t\tFATAL ERROR:\t" ICON_FA_EXCLAMATION));
                    ImGui::PopStyleColor();
                    lastFatalErrorIter = iter;

                    break;
                default:;
                }

                ImGui::SameLine();
                ImGui::TextUnformatted(iter->getMessage().c_str());
            }
        }

        if (lastFatalErrorIter != logger.cend())
        {
            ImGui::Begin("Something went terribly wrong");
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.f, 1.0f));
            ImGui::Text(reinterpret_cast<const char*>(ICON_FA_EXCLAMATION "\t\tA fatal error has occurred\t"));
            ImGui::PopStyleColor();
            ImGui::NewLine();
            ImGui::TextUnformatted(warpTextAfter(lastFatalErrorIter->getMessage(), 80).c_str());
            ImGui::NewLine();

            if (ImGui::Button("Quit application"))
            {
                exit(EXIT_FAILURE);
            }
            ImGui::End();
        }

        if (m_autoScroll)
        {
            ImGui::SetScrollHereY(0.9999f); // workaround for bug in SetScrollHereY see
                                            // https://github.com/ocornut/imgui/issues/1804
        }
        ImGui::EndChild();
        ImGui::End();
    }

    void LogView::updateCache(gladius::events::Logger & logger)
    {
        m_logSizeWhenCacheWasGenerated = logger.size();
        m_filteredEvents.clear();
        for (auto & item : logger)
        {
            if (m_filter.PassFilter(item.getMessage().c_str()))
            {
                m_filteredEvents.push_back(item);
            }
        }
    }
}
