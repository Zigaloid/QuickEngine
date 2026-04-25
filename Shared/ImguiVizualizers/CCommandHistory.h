#pragma once
#include "ICommand.h"
#include <memory>
#include <vector>

namespace ImGuiVisualizers {

/**
 * @brief Manages an undo/redo stack of ICommand objects.
 *
 * Usage:
 *   history.Push(std::make_unique<CSomeCommand>(...));  // records & executes
 *   history.Undo();
 *   history.Redo();
 *
 * Pushing a new command while there are redoable entries discards them,
 * consistent with standard editor behaviour.
 *
 * @param MaxDepth  Maximum number of commands retained (0 = unlimited).
 */
class CCommandHistory
{
public:
    explicit CCommandHistory(size_t maxDepth = 0) : m_maxDepth(maxDepth) {}

    // ── Core operations ────────────────────────────────────────────────

    /// Records and immediately executes a command.
    void Push(std::unique_ptr<ICommand> cmd);

    /// Undoes the most recently executed command. No-op when at the bottom.
    void Undo();

    /// Re-executes the most recently undone command. No-op when at the top.
    void Redo();

    /// Records a command that has **already been executed** externally.
    /// The command is placed on the stack at the current cursor position
    /// without calling Execute() again.
    void PushAlreadyExecuted(std::unique_ptr<ICommand> cmd);

    // ── State queries ──────────────────────────────────────────────────

    bool CanUndo() const { return m_cursor > 0; }
    bool CanRedo() const { return m_cursor < m_stack.size(); }

    /// Removes all history.
    void Clear();

    size_t GetUndoCount() const { return m_cursor; }
    size_t GetRedoCount() const { return m_stack.size() - m_cursor; }

    /// Label of the command that would be undone next (nullptr if none).
    const char* PeekUndoLabel() const;

    /// Label of the command that would be redone next (nullptr if none).
    const char* PeekRedoLabel() const;

private:
    std::vector<std::unique_ptr<ICommand>> m_stack;
    size_t                                 m_cursor   = 0; ///< Index of the next redo slot.
    size_t                                 m_maxDepth = 0;
};

} // namespace ImGuiVisualizers