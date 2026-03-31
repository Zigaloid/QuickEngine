#pragma once

#include "imgui.h"
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cctype>

#include "IImGuiVisualizer.h"

// Forward declaration
namespace Core { class CoreSystem; }
namespace FunctionCall { class FunctionCallManager; }

/**
 * @brief ImGui-based command console for executing registered commands
 * 
 * This console provides a text input interface for executing commands.
 * It maintains a command history, provides auto-completion, and supports
 * command suggestions based on registered command names.
 * It can automatically integrate with FunctionCallManager for command execution.
 */
class CommandConsole : public ImGuiVisualizers::IImGuiVisualizer
{
public:

    /**
 * @brief Called once after the visualizer is registered with the manager.
 * Use this for deferred setup that requires external systems to be ready.
 */
    void Initialize() {}

    /**
     * @brief Called once when the visualizer is unregistered or the manager
     * is destroyed. Use this to release resources.
     */
    void Shutdown() {}

    /**
     * @brief Called every frame regardless of visibility.
     * Use this for logic updates that must run continuously (e.g. data polling).
     * @param deltaTime Elapsed time in seconds since the last update.
     */
    void Update(float deltaTime) { (void)deltaTime; }

    /**
     * @brief Render the visualizer's ImGui window.
     * Only called when the visualizer is visible.
     * @param isOpen Pointer to a bool controlling window visibility.
     *              Set to false to hide the window.
     * @return true if the window was rendered, false otherwise.
     */
    bool Render(bool* isOpen)
    {
        RenderConsoleWindow();
        return true;
    }

    /**
     * @brief Return a display name used in menus and window titles.
     */
    const char* GetName() const
    {
        return "Command Console";
    }

    /**
     * @brief Optional keyboard shortcut label shown in the Windows menu.
     * Return nullptr if no shortcut is assigned.
     */
    virtual const char* GetShortcut() const
    {
        return "nullptr";
    }

    /**
     * @brief Optional menu category for grouping in the Windows menu.
     * Return nullptr to place the item at the root level.
     */
    virtual const char* GetMenuCategory() const
    {
        return "Show";
    }


    /**
     * @brief Command information structure
     */
    struct CommandInfo
    {
        std::string name;
        std::string description;
        
        CommandInfo(const std::string& cmdName, const std::string& cmdDesc = "")
            : name(cmdName), description(cmdDesc) {}
    };

    /**
     * @brief Callback function type for custom command execution
     * @param commandLine The full command line that was entered
     * @return true if command was handled, false otherwise
     */
    using CommandCallback = std::function<bool(const std::string& commandLine)>;

    /**
     * @brief Console message structure
     */
    struct ConsoleMessage
    {
        std::string text;
        ImU32 color;
        
        ConsoleMessage(const std::string& msg, ImU32 msgColor = IM_COL32(255, 255, 255, 255))
            : text(msg), color(msgColor) {}
    };

    bool IsVisable() const { return m_isVisable; }
    bool IsEnabled() const { return m_isEnabled; }
    void SetEnabled(bool val) 
    { 
        m_isEnabled = val; 
        if (m_isEnabled)
        {
            InitializeFunctionCallManager();
        }
    }
private:
    // Console state
    std::vector<CommandInfo> m_Commands;
    std::vector<std::string> m_CommandHistory;
    std::vector<ConsoleMessage> m_Messages;
    std::vector<std::string> m_Suggestions;
    
    // Input handling
    char m_InputBuffer[512];
    int m_HistoryPosition;
    bool m_ReclaimFocus;
    bool m_ScrollToBottom;
    bool m_AutoComplete;
    bool m_CaseSensitive;
    int m_MaxHistorySize;
    int m_MaxMessageCount;
    bool m_isVisable;
    // Callbacks
    CommandCallback m_CommandCallback;
    
    // Auto-completion
    std::string m_CurrentSuggestion;
    int m_SuggestionIndex;
    
    // FunctionCallManager integration
    FunctionCall::FunctionCallManager* m_FunctionManager;
    
    // Focus management
    bool m_WasWindowOpen;
    bool m_FocusInputOnNextFrame;
	bool m_isEnabled = false;
public:
    /**
     * @brief Constructor
     * @param maxHistorySize Maximum number of commands to keep in history (default: 100)
     * @param maxMessageCount Maximum number of messages to display (default: 1000)
     * @param caseSensitive Whether command matching is case sensitive (default: false)
     * @param useFunctionCallManager Whether to integrate with CoreSystem's FunctionCallManager (default: true)
     */
    explicit CommandConsole(int maxHistorySize = 100, int maxMessageCount = 1000, 
                           bool caseSensitive = false, bool useFunctionCallManager = true);

    /**
     * @brief Set the custom command execution callback
     * This is called for commands not handled by FunctionCallManager
     * @param callback Function to call when a command is entered
     */
    void SetCommandCallback(CommandCallback callback);

    /**
     * @brief Refresh commands from FunctionCallManager
     * Call this to update the console with newly registered functions
     */
    void RefreshFunctionCallManagerCommands();

    /**
     * @brief Register a custom command for auto-completion and help
     * @param name Command name
     * @param description Optional description for help display
     */
    void RegisterCommand(const std::string& name, const std::string& description = "");

    /**
     * @brief Register multiple commands at once
     * @param commands Vector of command info structures
     */
    void RegisterCommands(const std::vector<CommandInfo>& commands);

    /**
     * @brief Unregister a command
     * @param name Command name to remove
     */
    void UnregisterCommand(const std::string& name);

    /**
     * @brief Clear all registered commands
     */
    void ClearCommands();

    /**
     * @brief Add a message to the console output
     * @param message Message text
     * @param color Message color (default: white)
     */
    void AddMessage(const std::string& message, ImU32 color = IM_COL32(255, 255, 255, 255));

    /**
     * @brief Add an info message (white text)
     */
    void AddInfo(const std::string& message);

    /**
     * @brief Add a warning message (yellow text)
     */
    void AddWarning(const std::string& message);

    /**
     * @brief Add an error message (red text)
     */
    void AddError(const std::string& message);

    /**
     * @brief Add a success message (green text)
     */
    void AddSuccess(const std::string& message);

    /**
     * @brief Clear all console messages
     */
    void ClearMessages();

    /**
     * @brief Clear command history
     */
    void ClearHistory();

    /**
     * @brief Clear everything (messages, history, commands)
     */
    void ClearAll();

    /**
     * @brief Set whether auto-completion is enabled
     */
    void SetAutoComplete(bool enabled) { m_AutoComplete = enabled; }

    /**
     * @brief Check if auto-completion is enabled
     */
    bool IsAutoCompleteEnabled() const { return m_AutoComplete; }

    /**
     * @brief Set case sensitivity for command matching
     */
    void SetCaseSensitive(bool caseSensitive) { m_CaseSensitive = caseSensitive; }

    /**
     * @brief Check if case sensitive matching is enabled
     */
    bool IsCaseSensitive() const { return m_CaseSensitive; }

    /**
     * @brief Get the current command history
     */
    const std::vector<std::string>& GetCommandHistory() const { return m_CommandHistory; }

    /**
     * @brief Get the registered commands
     */
    const std::vector<CommandInfo>& GetRegisteredCommands() const { return m_Commands; }

    /**
     * @brief Render the console window
     * @param windowTitle Title for the ImGui window
     * @param isOpen Pointer to bool controlling window visibility
     * @return true if window is open and rendering
     */
    bool RenderConsoleWindow(const char* windowTitle = "Console", bool* isOpen = nullptr);

private:
    /**
     * @brief Handle text input callback for command line
     */
    static int TextEditCallback(ImGuiInputTextCallbackData* data);

    /**
     * @brief Execute a command
     */
    void ExecuteCommand(const std::string& commandLine);

    /**
     * @brief Try to execute command through FunctionCallManager
     * @return true if command was handled by FunctionCallManager
     */
    bool TryExecuteFunctionManagerCommand(const std::string& commandLine);

    /**
     * @brief Update command suggestions based on current input
     */
    void UpdateSuggestions(const std::string& input);

    /**
     * @brief Get the next suggestion for auto-completion
     */
    std::string GetNextSuggestion();

    /**
     * @brief Get the previous suggestion for auto-completion
     */
    std::string GetPreviousSuggestion();

    /**
     * @brief Utility function to convert string to lowercase
     */
    std::string ToLower(const std::string& str) const;

    /**
     * @brief Check if a string starts with another string (case-insensitive option)
     */
    bool StartsWith(const std::string& str, const std::string& prefix) const;

    /**
     * @brief Trim whitespace from a string
     */
    std::string Trim(const std::string& str) const;

    /**
     * @brief Render the message history
     */
    void RenderMessages();

    /**
     * @brief Render the input line with suggestions
     */
    void RenderInputLine();

    /**
     * @brief Render command suggestions
     */
    void RenderSuggestions();

    /**
     * @brief Initialize FunctionCallManager integration
     */
    void InitializeFunctionCallManager();
};