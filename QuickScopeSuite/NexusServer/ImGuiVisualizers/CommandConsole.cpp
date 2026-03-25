#include "CommandConsole.h"
#include "Profiler/Profiler.h"
#include "CoreSystem/CoreSystem.h"
#include "CoreSystem/FunctionCallManager.h"
#include <sstream>
#include <iomanip>

CommandConsole::CommandConsole(int maxHistorySize, int maxMessageCount, bool caseSensitive, bool useFunctionCallManager)
    : m_HistoryPosition(-1)
    , m_ReclaimFocus(false)
    , m_ScrollToBottom(false)
    , m_AutoComplete(true)
    , m_CaseSensitive(caseSensitive)
    , m_MaxHistorySize(maxHistorySize)
    , m_MaxMessageCount(maxMessageCount)
    , m_SuggestionIndex(-1)
    , m_FunctionManager(nullptr)
    , m_WasWindowOpen(false)
    , m_FocusInputOnNextFrame(false)
    , m_isVisable(false)
{
    DECLARE_FUNC_VLOW();
    memset(m_InputBuffer, 0, sizeof(m_InputBuffer));
        
    // Add welcome message
    AddInfo("Console initialized. Type 'help' for available commands.");    
}

void CommandConsole::SetCommandCallback(CommandCallback callback)
{
    DECLARE_FUNC_VLOW();
    m_CommandCallback = callback;
}

void CommandConsole::RefreshFunctionCallManagerCommands()
{
    DECLARE_FUNC_VLOW();
    
    if (!m_FunctionManager) {
        return;
    }
    
    // Remove existing FunctionCallManager commands
    m_Commands.erase(
        std::remove_if(m_Commands.begin(), m_Commands.end(),
            [](const CommandInfo& cmd) {
                return cmd.description == "FunctionCallManager command";
            }),
        m_Commands.end());
    
    // Add current FunctionCallManager commands
    auto registeredFunctions = m_FunctionManager->GetRegisteredFunctions();
    
    for (const auto& funcSignature : registeredFunctions) {
        // Extract function name from signature (before the first '(')
        size_t parenPos = funcSignature.find('(');
        std::string funcName = (parenPos != std::string::npos) ? 
            funcSignature.substr(0, parenPos) : funcSignature;
        
        RegisterCommand(funcName, "FunctionCallManager command");
    }    
    AddInfo("FunctionCallManager commands refreshed.");
}

void CommandConsole::InitializeFunctionCallManager()
{
    DECLARE_FUNC_VLOW();
    
    // Get FunctionCallManager from CoreSystem
    m_FunctionManager = Core::CoreSystem::GetFunctionManager();
    if (m_FunctionManager)
    {
        RefreshFunctionCallManagerCommands();
    }
    else
    {
        AddWarning("CoreSystem not initialized. FunctionCallManager integration disabled.");        
    }
}

void CommandConsole::RegisterCommand(const std::string& name, const std::string& description)
{
    DECLARE_FUNC_VLOW();
    // Check if command already exists
    auto it = std::find_if(m_Commands.begin(), m_Commands.end(),
        [&name, this](const CommandInfo& cmd) {
            return m_CaseSensitive ? cmd.name == name : ToLower(cmd.name) == ToLower(name);
        });
    
    if (it != m_Commands.end()) {
        // Update existing command
        it->description = description;
    } else {
        // Add new command
        m_Commands.emplace_back(name, description);
    }
}

void CommandConsole::RegisterCommands(const std::vector<CommandInfo>& commands)
{
    DECLARE_FUNC_VLOW();
    for (const auto& cmd : commands) {
        RegisterCommand(cmd.name, cmd.description);
    }
}

void CommandConsole::UnregisterCommand(const std::string& name)
{
    DECLARE_FUNC_VLOW();
    m_Commands.erase(
        std::remove_if(m_Commands.begin(), m_Commands.end(),
            [&name, this](const CommandInfo& cmd) {
                return m_CaseSensitive ? cmd.name == name : ToLower(cmd.name) == ToLower(name);
            }),
        m_Commands.end());
}

void CommandConsole::ClearCommands()
{
    DECLARE_FUNC_VLOW();
    m_Commands.clear();
}

void CommandConsole::AddMessage(const std::string& message, ImU32 color)
{
    DECLARE_FUNC_VLOW();
    m_Messages.emplace_back(message, color);
    
    // Limit message count
    if (m_Messages.size() > static_cast<size_t>(m_MaxMessageCount)) {
        m_Messages.erase(m_Messages.begin(), m_Messages.begin() + (m_Messages.size() - m_MaxMessageCount));
    }
    
    m_ScrollToBottom = true;
}

void CommandConsole::AddInfo(const std::string& message)
{
    AddMessage(message, IM_COL32(255, 255, 255, 255)); // White
}

void CommandConsole::AddWarning(const std::string& message)
{
    AddMessage(message, IM_COL32(255, 255, 0, 255)); // Yellow
}

void CommandConsole::AddError(const std::string& message)
{
    AddMessage(message, IM_COL32(255, 100, 100, 255)); // Red
}

void CommandConsole::AddSuccess(const std::string& message)
{
    AddMessage(message, IM_COL32(100, 255, 100, 255)); // Green
}

void CommandConsole::ClearMessages()
{
    DECLARE_FUNC_VLOW();
    m_Messages.clear();
}

void CommandConsole::ClearHistory()
{
    DECLARE_FUNC_VLOW();
    m_CommandHistory.clear();
    m_HistoryPosition = -1;
}

void CommandConsole::ClearAll()
{
    DECLARE_FUNC_VLOW();
    ClearMessages();
    ClearHistory();
    ClearCommands();
}

bool CommandConsole::RenderConsoleWindow(const char* windowTitle, bool* isOpen)
{
    DECLARE_FUNC_LOW();
    
    // Check if window should be open
    bool windowShouldBeOpen = (isOpen == nullptr) || *isOpen;
    
    if (!windowShouldBeOpen) {
        m_WasWindowOpen = false;
        return false;
    }
    
    // Detect if window is being opened this frame
    bool windowJustOpened = !m_WasWindowOpen && windowShouldBeOpen;
    m_WasWindowOpen = windowShouldBeOpen;
    
    // If window was just opened, request focus on next frame
    if (windowJustOpened) {
        m_FocusInputOnNextFrame = true;
    }
    
    if (!ImGui::Begin(windowTitle, isOpen, ImGuiWindowFlags_None)) {
        ImGui::End();
        return false;
    }
    
    // Reserve space for the input line at the bottom
    const float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    
    // Messages area
    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeightToReserve), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        RenderMessages();
    }
    ImGui::EndChild();
    
    // Input line
    ImGui::Separator();
    RenderInputLine();
    
    // Suggestions
    if (m_AutoComplete && !m_Suggestions.empty()) {
        RenderSuggestions();
    }
    
    ImGui::End();
    return true;
}

void CommandConsole::RenderMessages()
{
    DECLARE_FUNC_LOW();
    
    for (const auto& message : m_Messages) {
        ImGui::PushStyleColor(ImGuiCol_Text, message.color);
        ImGui::TextWrapped("%s", message.text.c_str());
        ImGui::PopStyleColor();
    }
    
    // Auto-scroll to bottom
    if (m_ScrollToBottom) {
        ImGui::SetScrollHereY(1.0f);
        m_ScrollToBottom = false;
    }
}

void CommandConsole::RenderInputLine()
{
    DECLARE_FUNC_LOW();
    
    // Input field
    ImGui::PushItemWidth(-1);
    
    bool reclaimFocus = false;
    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | 
                                    ImGuiInputTextFlags_CallbackCompletion | 
                                    ImGuiInputTextFlags_CallbackHistory |
                                    ImGuiInputTextFlags_CallbackAlways; // Add this flag to get callbacks on every change
    
    if (ImGui::InputText("##Input", m_InputBuffer, sizeof(m_InputBuffer), inputFlags, 
                         &CommandConsole::TextEditCallback, (void*)this)) {
        std::string command = Trim(std::string(m_InputBuffer));
        if (!command.empty()) {
            ExecuteCommand(command);
            strcpy_s(m_InputBuffer, "");
            // Clear suggestions after command execution
            m_Suggestions.clear();
            m_SuggestionIndex = -1;
        }
        reclaimFocus = true;
    }
    
    ImGui::PopItemWidth();
    
    // Auto-focus logic
    bool shouldFocus = false;
    
    // Focus if window was just opened
    if (m_FocusInputOnNextFrame) {
        shouldFocus = true;
        m_FocusInputOnNextFrame = false;
    }
    
    // Focus after command execution
    if (reclaimFocus || m_ReclaimFocus) {
        shouldFocus = true;
        m_ReclaimFocus = false;
    }
    
    // Focus when window appears or gains focus for the first time
    if (ImGui::IsWindowAppearing()) {
        shouldFocus = true;
    }
    
    // Apply focus
    if (shouldFocus) {
        ImGui::SetKeyboardFocusHere(-1); // Focus the previous item (the InputText)
    }
    
    // Set as default focus for this window
    ImGui::SetItemDefaultFocus();
}

void CommandConsole::RenderSuggestions()
{
    DECLARE_FUNC_LOW();
    
    if (m_Suggestions.empty()) return;
    
    ImGui::Separator();
    ImGui::Text("Suggestions:");
    ImGui::Indent();
    
    for (size_t i = 0; i < m_Suggestions.size() && i < 10; ++i) { // Limit to 10 suggestions
        const std::string& suggestion = m_Suggestions[i];
        
        // Highlight current suggestion
        if (static_cast<int>(i) == m_SuggestionIndex) {
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(100, 255, 100, 255));
        }
        
        if (ImGui::Selectable(suggestion.c_str())) {
            strcpy_s(m_InputBuffer, suggestion.c_str());
            m_ReclaimFocus = true;
        }
        
        if (static_cast<int>(i) == m_SuggestionIndex) {
            ImGui::PopStyleColor();
        }
    }
    
    ImGui::Unindent();
}

int CommandConsole::TextEditCallback(ImGuiInputTextCallbackData* data)
{
    CommandConsole* console = static_cast<CommandConsole*>(data->UserData);
    
    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackCompletion:
        {
            // Auto-completion with Tab key
            std::string currentInput(data->Buf, data->BufTextLen);
            console->UpdateSuggestions(currentInput);
            
            if (!console->m_Suggestions.empty()) {
                std::string suggestion = console->GetNextSuggestion();
                if (!suggestion.empty()) {
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, suggestion.c_str());
                }
            }
        }
        break;
        
    case ImGuiInputTextFlags_CallbackHistory:
        {
            // Command history with Up/Down arrows
            const int prevHistoryPos = console->m_HistoryPosition;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (console->m_HistoryPosition == -1) {
                    console->m_HistoryPosition = static_cast<int>(console->m_CommandHistory.size()) - 1;
                } else if (console->m_HistoryPosition > 0) {
                    console->m_HistoryPosition--;
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (console->m_HistoryPosition != -1) {
                    console->m_HistoryPosition++;
                    if (console->m_HistoryPosition >= static_cast<int>(console->m_CommandHistory.size())) {
                        console->m_HistoryPosition = -1;
                    }
                }
            }
            
            // Apply history change
            if (prevHistoryPos != console->m_HistoryPosition) {
                const char* historyStr = (console->m_HistoryPosition >= 0) ? 
                    console->m_CommandHistory[console->m_HistoryPosition].c_str() : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, historyStr);
                
                // Update suggestions after history change
                std::string newInput(historyStr);
                console->UpdateSuggestions(newInput);
            }
        }
        break;
        
    case ImGuiInputTextFlags_CallbackAlways:
        {
            // This callback is triggered on every text change
            std::string currentInput(data->Buf, data->BufTextLen);
            console->UpdateSuggestions(currentInput);
        }
        break;
    }
    
    return 0;
}

void CommandConsole::ExecuteCommand(const std::string& commandLine)
{
    DECLARE_FUNC_LOW();
    
    // Add to history
    if (!commandLine.empty()) {
        // Remove duplicate if it exists
        auto it = std::find(m_CommandHistory.begin(), m_CommandHistory.end(), commandLine);
        if (it != m_CommandHistory.end()) {
            m_CommandHistory.erase(it);
        }
        
        // Add to end
        m_CommandHistory.push_back(commandLine);
        
        // Limit history size
        if (m_CommandHistory.size() > static_cast<size_t>(m_MaxHistorySize)) {
            m_CommandHistory.erase(m_CommandHistory.begin());
        }
    }
    m_HistoryPosition = -1;
    
    // Display command in console
    AddMessage("> " + commandLine, IM_COL32(200, 200, 200, 255));
    
    // Handle built-in commands
    std::string command = ToLower(Trim(commandLine));
    
    if (command == "help" || command == "?") {
        AddInfo("Available commands:");
        for (const auto& cmd : m_Commands) {
            std::string helpText = "  " + cmd.name;
            if (!cmd.description.empty()) {
                helpText += " - " + cmd.description;
            }
            AddInfo(helpText);
        }
        AddInfo("Built-in commands:");
        AddInfo("  help, ? - Show this help");
        AddInfo("  clear - Clear console messages");
        AddInfo("  history - Show command history");
        AddInfo("  refresh - Refresh FunctionCallManager commands");
        return;
    }
    
    if (command == "clear") {
        ClearMessages();
        AddInfo("Console cleared.");
        return;
    }
    
    if (command == "history") {
        AddInfo("Command history:");
        for (size_t i = 0; i < m_CommandHistory.size(); ++i) {
            AddInfo("  " + std::to_string(i + 1) + ": " + m_CommandHistory[i]);
        }
        return;
    }
    
    if (command == "refresh") {
        RefreshFunctionCallManagerCommands();
        return;
	}

    // Try to execute through FunctionCallManager first
    if (TryExecuteFunctionManagerCommand(commandLine)) {
        return; // Command was handled by FunctionCallManager
    }
    
    // Try to execute through custom callback
    bool handled = false;
    if (m_CommandCallback) {
        try {
            handled = m_CommandCallback(commandLine);
        } catch (const std::exception& e) {
            AddError("Command execution failed: " + std::string(e.what()));
            return;
        }
    }
    
    if (!handled) {
        AddError("Unknown command: " + commandLine);
        AddInfo("Type 'help' for available commands.");
    }
}

bool CommandConsole::TryExecuteFunctionManagerCommand(const std::string& commandLine)
{
    DECLARE_FUNC_LOW();
    
    if (!m_FunctionManager) {
        return false;
    }
    
    try {
        auto result = m_FunctionManager->CallFunction<void>(commandLine);
        if (result.IsSuccess()) {
            AddSuccess("Command executed successfully");
            return true;
        } else {
            AddError("Command failed: " + result.GetError());
            return true; // We handled it, even if it failed
        }
    } catch (const std::exception& e) {
        AddError("FunctionCallManager error: " + std::string(e.what()));
        return true; // We handled it, even if it failed
    }
}

void CommandConsole::UpdateSuggestions(const std::string& input)
{
    DECLARE_FUNC_LOW();
    
    m_Suggestions.clear();
    m_SuggestionIndex = -1;
    
    if (input.empty()) return;
    
    std::string trimmedInput = Trim(input);
    if (trimmedInput.empty()) return;
    
    // Find matching commands
    for (const auto& cmd : m_Commands) {
        if (StartsWith(cmd.name, trimmedInput)) {
            m_Suggestions.push_back(cmd.name);
        }
    }
    
    // Sort suggestions
    std::sort(m_Suggestions.begin(), m_Suggestions.end());
}

std::string CommandConsole::GetNextSuggestion()
{
    if (m_Suggestions.empty()) return "";
    
    m_SuggestionIndex = (m_SuggestionIndex + 1) % static_cast<int>(m_Suggestions.size());
    return m_Suggestions[m_SuggestionIndex];
}

std::string CommandConsole::GetPreviousSuggestion()
{
    if (m_Suggestions.empty()) return "";
    
    m_SuggestionIndex = (m_SuggestionIndex - 1 + static_cast<int>(m_Suggestions.size())) % static_cast<int>(m_Suggestions.size());
    return m_Suggestions[m_SuggestionIndex];
}

std::string CommandConsole::ToLower(const std::string& str) const
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

bool CommandConsole::StartsWith(const std::string& str, const std::string& prefix) const
{
    if (prefix.length() > str.length()) return false;
    
    if (m_CaseSensitive) {
        return str.compare(0, prefix.length(), prefix) == 0;
    } else {
        return ToLower(str).compare(0, prefix.length(), ToLower(prefix)) == 0;
    }
}

std::string CommandConsole::Trim(const std::string& str) const
{
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}