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

       
        // Render appropriate view based on collapsed state
        if (m_collapsed)
        {
                        renderCollapsedView(logger);
        }
        else
        {
            // we need at least 400px height to show the toolbar and the log
   
            ImGui::SetNextWindowSizeConstraints(ImVec2(-1, 400), ImVec2(-1, FLT_MAX)); // Min height of 400px, no max height constraint
            ImGui::SetNextWindowSize(ImVec2(0, 400));

             ImGui::Begin("Events", &m_visible);
            // Top toolbar
            ImGui::Checkbox("Auto-scroll", &m_autoScroll);
            ImGui::SameLine();
            
            if (ImGui::Button("Collapse"))
            {
                m_collapsed = true;
                // Force update of cache when switching view modes
                updateCache(logger);
            }
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

        
            renderExpandedView(logger);
            ImGui::End();
        }  
    
    }

    void LogView::renderCollapsedView(gladius::events::Logger & logger)
    {
        auto const & eventsBegin = m_filter.IsActive() ? m_filteredEvents.cbegin() : logger.cbegin();
        auto const & eventsEnd = m_filter.IsActive() ? m_filteredEvents.cend() : logger.cend();

        // remove window size constraints to allow resizing
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(-1, FLT_MAX)); // No min/max constraints
        ImGui::SetNextWindowSize(ImVec2(500, 50));
             
        ImGui::Begin("Events", &m_visible);
        
        // Count warnings, errors and fatal errors
        size_t infoCount = 0;
        size_t warningCount = 0;
        size_t errorCount = 0;
        size_t fatalErrorCount = 0;
        bool hasFatalError = false;
        
        for (auto iter = eventsBegin; iter != eventsEnd; ++iter)
        {
            switch (iter->getSeverity())
            {
            case events::Severity::Info:
                infoCount++;
                break;
            case events::Severity::Warning:
                warningCount++;
                break;
            case events::Severity::Error:
                errorCount++;
                break;
            case events::Severity::FatalError:
                fatalErrorCount++;
                hasFatalError = true;
                break;
            default:;
            }
        }        if (fatalErrorCount > 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.f, 1.0f));
            ImGui::Text(reinterpret_cast<const char*>(ICON_FA_EXCLAMATION " Fatal Errors: %zu"), fatalErrorCount);
            ImGui::PopStyleColor();
            
            // Show tooltip with all fatal error messages when hovering
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.f, 1.0f));
                ImGui::TextUnformatted("Fatal Errors:");
                ImGui::PopStyleColor();
                ImGui::Separator();
                
                for (auto iter = eventsBegin; iter != eventsEnd; ++iter)
                {
                    if (iter->getSeverity() == events::Severity::FatalError)
                    {
                        ImGui::TextUnformatted(iter->getMessage().c_str());
                        ImGui::Separator();
                    }
                }
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
        }

        if (errorCount > 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
            ImGui::Text(reinterpret_cast<const char*>(ICON_FA_EXCLAMATION_TRIANGLE " Errors: %zu"), errorCount);
            ImGui::PopStyleColor();
            
            // Show tooltip with all error messages when hovering
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                ImGui::TextUnformatted("Errors:");
                ImGui::PopStyleColor();
                ImGui::Separator();
                
                for (auto iter = eventsBegin; iter != eventsEnd; ++iter)
                {
                    if (iter->getSeverity() == events::Severity::Error)
                    {
                        ImGui::TextUnformatted(iter->getMessage().c_str());
                        ImGui::Separator();
                    }
                }
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
        }

        if (warningCount > 0)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.f, 1.0f));
            ImGui::Text(reinterpret_cast<const char*>(ICON_FA_EXCLAMATION_CIRCLE " Warnings: %zu"), warningCount);
            ImGui::PopStyleColor();
            
            // Show tooltip with all warning messages when hovering
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.f, 1.0f));
                ImGui::TextUnformatted("Warnings:");
                ImGui::PopStyleColor();
                ImGui::Separator();
                
                for (auto iter = eventsBegin; iter != eventsEnd; ++iter)
                {
                    if (iter->getSeverity() == events::Severity::Warning)
                    {
                        ImGui::TextUnformatted(iter->getMessage().c_str());
                        ImGui::Separator();
                    }
                }
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
        }

        if (ImGui::Button("Clear"))
        {
            logger.clear();
            hide();
        }
         
        if (ImGui::Button("Show Log"))
        {
            m_collapsed = false;
            // Force update of cache when switching view modes
            updateCache(logger);
        }


        // Show the fatal error dialog if needed
        if (hasFatalError)
        {
            auto lastFatalErrorIter = eventsEnd;
            for (auto iter = eventsBegin; iter != eventsEnd; ++iter)
            {
                if (iter->getSeverity() == events::Severity::FatalError)
                {
                    lastFatalErrorIter = iter;
                }
            }
            
            if (lastFatalErrorIter != eventsEnd)
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
        }
        ImGui::End();
    }

    void LogView::renderExpandedView(gladius::events::Logger & logger)
    {
        auto const & eventsBegin = m_filter.IsActive() ? m_filteredEvents.cbegin() : logger.cbegin();
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
