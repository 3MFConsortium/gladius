#pragma once

#include "Assembly.h"
#include "nodesfwd.h"

#include <string>
#include <vector>

namespace gladius::nodes
{
    struct HistoryItem
    {
        Assembly assembly;
        std::string description = {"unknown action"};
    };

    using UndoStack = std::vector<HistoryItem>;

    class History
    {
      public:
        void storeState(Assembly const & assembly, std::string const & description);
        void undo(Assembly * assembly);
        void redo(Assembly * assembly);

        [[nodiscard]] bool canUnDo() const;
        [[nodiscard]] bool canReDo() const;

      private:
        static void applyPreviousState(Assembly * assembly, UndoStack & stack);
        void storeStateInternal(Assembly const & assembly, std::string const & description);

        UndoStack m_undoStack;
        UndoStack m_redoStack;
    };
} // namespace gladius::nodes