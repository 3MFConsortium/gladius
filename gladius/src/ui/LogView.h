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

        bool m_visible = false;
        bool m_autoScroll = true;
        bool m_collapsed = true;
        ImGuiTextFilter m_filter;
        events::Events m_filteredEvents;
        size_t m_logSizeWhenCacheWasGenerated = 0u;
    };
}
