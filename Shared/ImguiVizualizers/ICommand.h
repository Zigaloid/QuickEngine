#pragma once

namespace ImGuiVisualizers {

/**
 * @brief Abstract base for all undoable/redoable operations.
 *
 * Implement Execute() to (re-)apply the operation and Undo() to revert it.
 * CCommandHistory calls Execute() when the command is first pushed, then
 * alternates between Undo() / Execute() as the user steps through history.
 */
class ICommand
{
public:
    virtual ~ICommand() = default;

    /// Apply (or re-apply) this operation.
    virtual void Execute() = 0;

    /// Revert this operation.
    virtual void Undo() = 0;

    /// Human-readable label shown in debug UIs (optional).
    virtual const char* GetLabel() const { return "Command"; }
};

} // namespace ImGuiVisualizers