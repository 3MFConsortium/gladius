#pragma once

#include "../EventLogger.h"

#include "imgui.h"

namespace gladius::ui
{
    class LogView
    {
      public:
        void show();

        void hide();

        void render(events::Logger & logger);

      private:
        void updateCache(events::Logger & logger);
        void renderCollapsedView(events::Logger & logger);
        void renderExpandedView(events::Logger & logger);
        bool isSeverityFilterActive() const
        {
            // Active when not all severities are enabled
            return !(m_showInfo && m_showWarnings && m_showErrors && m_showFatal);
        }

        bool m_visible = false;
        bool m_autoScroll = true;
        bool m_collapsed = false;
        ImGuiTextFilter m_filter;
        events::Events m_filteredEvents;
        size_t m_logSizeWhenCacheWasGenerated = 0u;

        // Severity filter state (all enabled by default)
        bool m_showInfo = true;
        bool m_showWarnings = true;
        bool m_showErrors = true;
        bool m_showFatal = true;
    };
}
