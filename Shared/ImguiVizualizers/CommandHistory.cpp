#include "CommandHistory.h"
#include <cassert>

namespace ImGuiVisualizers {

void CCommandHistory::Push(std::unique_ptr<ICommand> cmd)
{
    assert(cmd);

    // Discard any commands ahead of the current cursor (the "redo" tail).
    m_stack.erase(m_stack.begin() + static_cast<std::ptrdiff_t>(m_cursor), m_stack.end());

    cmd->Execute();
    m_stack.push_back(std::move(cmd));
    ++m_cursor;

    // Enforce depth limit by dropping the oldest entry.
    if (m_maxDepth > 0 && m_stack.size() > m_maxDepth)
    {
        m_stack.erase(m_stack.begin());
        --m_cursor;
    }
}

void CCommandHistory::Undo()
{
    if (!CanUndo())
        return;

    --m_cursor;
    m_stack[m_cursor]->Undo();
}

void CCommandHistory::Redo()
{
    if (!CanRedo())
        return;

    m_stack[m_cursor]->Execute();
    ++m_cursor;
}

void CCommandHistory::Clear()
{
    m_stack.clear();
    m_cursor = 0;
}

const char* CCommandHistory::PeekUndoLabel() const
{
    return CanUndo() ? m_stack[m_cursor - 1]->GetLabel() : nullptr;
}

const char* CCommandHistory::PeekRedoLabel() const
{
    return CanRedo() ? m_stack[m_cursor]->GetLabel() : nullptr;
}

void CCommandHistory::PushAlreadyExecuted(std::unique_ptr<ICommand> cmd)
{
    assert(cmd);

    m_stack.erase(m_stack.begin() + static_cast<std::ptrdiff_t>(m_cursor), m_stack.end());
    m_stack.push_back(std::move(cmd));
    ++m_cursor;

    if (m_maxDepth > 0 && m_stack.size() > m_maxDepth)
    {
        m_stack.erase(m_stack.begin());
        --m_cursor;
    }
}

} // namespace ImGuiVisualizers