#include "History.h"
#include "Assembly.h"
#include "nodesfwd.h"

namespace gladius::nodes
{
    void History::storeState(Assembly const & assembly, std::string const & description)
    {
        storeStateInternal(assembly, description);
        m_redoStack.clear();
    }

    void History::undo(Assembly * assembly)
    {
        m_redoStack.push_back({*assembly, "Undo"});
        applyPreviousState(assembly, m_undoStack);
    }

    void History::redo(Assembly * assembly)
    {
        storeStateInternal(*assembly, "Redo");
        applyPreviousState(assembly, m_redoStack);
    }

    bool History::canUnDo() const
    {
        return !m_undoStack.empty();
    }

    bool History::canReDo() const
    {
        return !m_redoStack.empty();
    }

    void History::applyPreviousState(Assembly * assembly, UndoStack & stack)
    {
        if (assembly == nullptr || stack.empty())
        {
            // No action possible; avoid noisy console output
            return;
        }

        *assembly = stack.back().assembly;
        stack.pop_back();
    }

    void History::storeStateInternal(Assembly const & assembly, std::string const & description)
    {
        if (!m_undoStack.empty())
        {
            if (m_undoStack.back().assembly.equals(assembly))
            {
                return;
            }
        }
        m_undoStack.push_back({assembly, description});
    }

} // namespace gladius::nodes