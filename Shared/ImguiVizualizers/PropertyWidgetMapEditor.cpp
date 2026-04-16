#include "PropertyWidgetMapEditor.h"
#include "imgui.h"
#include "ClassFactory/ClassFactory.h"
#include "Reflection/ReflectionMap.h"
#include "Reflection/Reflection.h"
#include "Reflection/ReflectionError.h" // for REFL_ERROR / ErrorCategory

#include <algorithm>
#include <cctype>
#include <memory>

namespace ImGuiVisualizers {

    static std::string ToLower(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        return out;
    }

    static const char* EditorWidgetTypeToString(EditorWidgetType t) {
        switch (t) {
        case EditorWidgetType::Default: return "Default";
        case EditorWidgetType::InputField: return "InputField";
        case EditorWidgetType::Slider: return "Slider";
        case EditorWidgetType::Drag: return "Drag";
        case EditorWidgetType::ColorPicker: return "ColorPicker";
        case EditorWidgetType::Checkbox: return "Checkbox";
        case EditorWidgetType::Dropdown: return "Dropdown";
        case EditorWidgetType::TextArea: return "TextArea";
        case EditorWidgetType::FilePicker: return "FilePicker";
        case EditorWidgetType::ReadOnly: return "ReadOnly";
        default: return "Unknown";
        }
    }

    static const char* s_WidgetTypeNames[] = {
        "Default","InputField","Slider","Drag","ColorPicker","Checkbox","Dropdown","TextArea","FilePicker","ReadOnly"
    };

    PropertyWidgetMapEditor::PropertyWidgetMapEditor()
    {
        RegisterEditorActions();
    }

    PropertyWidgetMapEditor::~PropertyWidgetMapEditor()
    {
        UnregisterEditorActions();
    }

    void PropertyWidgetMapEditor::Initialize()
    {
        // Do not auto-load any file here; allow caller to SetPath explicitly.
        RefreshClassList();
    }

    void PropertyWidgetMapEditor::Shutdown()
    {
        UnregisterEditorActions();

        m_classNames.clear();
        m_selectedClassIndex = -1;
        m_selectedClassName.clear();
        m_memberNames.clear();
        m_selectedMemberIndex = -1;
        m_tempInstance.reset();
        m_currentPath.clear();
        m_filterBuf[0] = '\0';
        m_editWidgetType = EditorWidgetType::Default;
        m_editHasConfig = false;
        m_editConfig = WidgetConfig();
    }

    void PropertyWidgetMapEditor::RefreshClassList()
    {
        m_classNames = ClassFactory::GetRegisteredClassNames();
        std::sort(m_classNames.begin(), m_classNames.end());
        m_selectedClassIndex = -1;
        m_selectedClassName.clear();
        m_memberNames.clear();
        m_selectedMemberIndex = -1;
        m_tempInstance.reset();
    }

    // Simple namespaced key so DocumentManager can track open editors uniquely.
    std::string PropertyWidgetMapEditor::MakeDocumentKey(const std::string& assetPath)
    {
        return std::string("PropertyWidgetMapEditor:") + assetPath;
    }

    bool PropertyWidgetMapEditor::SetPath(const std::string& path)
    {
        if (path.empty()) {
            REFL_ERROR(Reflection::ErrorCategory::FileIO, "Empty path passed to PropertyWidgetMapEditor::SetPath", path);
            return false;
        }

        auto result = PropertyWidgetMapRegistry::Instance().SafeRead(path);
        if (result.IsSuccess()) {
            m_currentPath = path;
            // The registry may have new class names / maps; refresh our snapshot.
            RefreshClassList();
            return true;
        }
        else {
            // Log the underlying reflection error information.
            const auto& err = result.GetError();
            REFL_ERROR(Reflection::ErrorCategory::FileIO, "Failed to load widgets file", err.ToString());
            return false;
        }
    }

    bool PropertyWidgetMapEditor::Save()
    {
        if (m_currentPath.empty()) {
            REFL_ERROR(Reflection::ErrorCategory::FileIO, "No path set for Save()", "Call SetPath(...) before Save()");
            return false;
        }

        auto result = PropertyWidgetMapRegistry::Instance().SafeWrite(m_currentPath);
        if (result.IsSuccess()) {
            return true;
        }
        else {
            const auto& err = result.GetError();
            REFL_ERROR(Reflection::ErrorCategory::FileIO, "Failed to write widgets file", err.ToString());
            return false;
        }
    }

    void PropertyWidgetMapEditor::PopulateMemberList(const std::string& className)
    {
        m_memberNames.clear();
        m_selectedMemberIndex = -1;
        m_tempInstance.reset();

        if (className.empty()) return;

        // Create a temporary reflected instance via ClassFactory
        CReflectedBase* raw = ClassFactory::CreateObject(className.c_str());
        if (!raw) {
            REFL_ERROR(Reflection::ErrorCategory::ClassFactory, "ClassFactory failed to create instance", className);
            return;
        }

        // adopt ownership
        m_tempInstance.reset(raw);

        // Collect hierarchy reflection maps
        std::vector<std::pair<const char*, std::vector<CReflectionMapEntry>*>> hierarchy;
        m_tempInstance->CollectHierarchyReflectionMaps(hierarchy);

        // Find the map that corresponds exactly to the class (root members)
        for (auto& pair : hierarchy) {
            if (pair.first && className == pair.first) {
                auto* mapVec = pair.second;
                if (!mapVec) break;
                for (const auto& entry : *mapVec) {
                    if (entry.GetProperty()) {
                        m_memberNames.push_back(entry.GetProperty()->GetName());
                    }
                }
                break;
            }
        }
    }

    // Load mapping for selected member into UI-edit state
    void PropertyWidgetMapEditor::LoadSelectedMemberMapping()
    {
        m_editWidgetType = EditorWidgetType::Default;
        m_editHasConfig = false;
        m_editConfig = WidgetConfig();

        if (m_selectedClassIndex < 0 || m_selectedMemberIndex < 0) return;
        const std::string& cls = m_selectedClassName;
        const std::string& member = m_memberNames[m_selectedMemberIndex];

        auto map = PropertyWidgetMapRegistry::Instance().Get(cls);
        if (!map) return;

        if (map->HasCustomWidget(member)) {
            m_editWidgetType = map->GetWidget(member);
            const WidgetConfig* cfg = map->GetConfig(member);
            if (cfg) {
                m_editHasConfig = true;
                m_editConfig = *cfg;
            }
            else {
                m_editHasConfig = false;
                m_editConfig = WidgetConfig();
            }
        }
        else {
            m_editWidgetType = EditorWidgetType::Default;
            m_editHasConfig = false;
            m_editConfig = WidgetConfig();
        }
    }

    // Apply UI-edit state back to registry for the selected member
    void PropertyWidgetMapEditor::ApplySelectedMemberMapping()
    {
        if (m_selectedClassIndex < 0 || m_selectedMemberIndex < 0) return;
        const std::string& cls = m_selectedClassName;
        const std::string& member = m_memberNames[m_selectedMemberIndex];

        // Ensure a PropertyWidgetMap exists for this class
        auto existing = PropertyWidgetMapRegistry::Instance().Get(cls);
        if (!existing) {
            // create and register
            auto newMap = std::make_shared<PropertyWidgetMap>();
            PropertyWidgetMapRegistry::Instance().Register(cls, newMap);
            existing = newMap;
        }

        // If Default chosen, remove mapping to fall back to default behavior
        if (m_editWidgetType == EditorWidgetType::Default) {
            existing->RemoveWidget(member);
            // update the serialized clone so Save() picks up the change
            PropertyWidgetMapRegistry::Instance().Register(cls, existing);
            return;
        }

        if (m_editHasConfig) {
            existing->SetWidget(member, m_editWidgetType, m_editConfig);
        }
        else {
            existing->SetWidget(member, m_editWidgetType);
        }

        // update the serialized clone so Save() picks up the change
        PropertyWidgetMapRegistry::Instance().Register(cls, existing);
    }

    void PropertyWidgetMapEditor::RegisterEditorActions()
    {
        // Save action
        m_actionManager.RegisterAction({
            .path = "File.Save",
            .description = "Save the current widget map to disk",
            .targets = UI::ActionTarget::Toolbar | UI::ActionTarget::Menu | UI::ActionTarget::Console,
            .callback = [this]() { Save(); },
            .isEnabled = [this]() { return !m_currentPath.empty(); },
            .sortPriority = 10
            });

        // Reload action
        m_actionManager.RegisterAction({
            .path = "File.Reload",
            .description = "Reload the widget map file (discard changes)",
            .targets = UI::ActionTarget::Toolbar | UI::ActionTarget::Menu | UI::ActionTarget::Console,
            .callback = [this]() { if (!m_currentPath.empty()) SetPath(m_currentPath); },
            .isEnabled = [this]() { return !m_currentPath.empty(); },
            .sortPriority = 20
            });

        // Refresh class list action (left-panel)
        m_actionManager.RegisterAction({
            .path = "File.RefreshClasses",
            .description = "Refresh the registered class list",
            .targets = UI::ActionTarget::Toolbar | UI::ActionTarget::Menu | UI::ActionTarget::Console,
            .callback = [this]() { RefreshClassList(); },
            .isEnabled = []() { return true; },
            .sortPriority = 30
            });
    }

    void PropertyWidgetMapEditor::UnregisterEditorActions()
    {
        m_actionManager.UnregisterAction("File.Save");
        m_actionManager.UnregisterAction("File.Reload");
        m_actionManager.UnregisterAction("File.RefreshClasses");
    }

    bool PropertyWidgetMapEditor::Render(bool* isOpen)
    {
        if (!ImGui::Begin(GetName(), isOpen, ImGuiWindowFlags_MenuBar)) {
            ImGui::End();
            return false;
        }
        // Render inspector inline
        if (ImGui::BeginMenuBar())
        {
            m_actionManager.RenderMenuBar();
            ImGui::EndMenuBar();
        }


        // Top bar: show toolbar and current path
        ImGui::BeginChild("##top_bar", ImVec2(0, 40), false);
        // Render registered toolbar actions
        m_actionManager.RenderToolbar();

        ImGui::SameLine();
        ImGui::TextWrapped("Path: %s", m_currentPath.empty() ? "<none>" : m_currentPath.c_str());
        ImGui::EndChild();

        ImGui::Separator();

        // Left panel: class list with filter
        ImGui::BeginChild("##left_panel", ImVec2(300, 0), true);

        // Filter + refresh
        ImGui::InputText("Filter", m_filterBuf, sizeof(m_filterBuf));
        ImGui::SameLine();
        if (ImGui::SmallButton("Refresh")) {
            RefreshClassList();
        }
        ImGui::Separator();

        const std::string filter = ToLower(std::string(m_filterBuf));
        for (size_t i = 0; i < m_classNames.size(); ++i) {
            const std::string& cls = m_classNames[i];
            if (!filter.empty()) {
                if (ToLower(cls).find(filter) == std::string::npos) continue;
            }

            bool selected = (static_cast<int>(i) == m_selectedClassIndex);
            if (ImGui::Selectable(m_classNames[i].c_str(), selected)) {
                if (!selected) {
                    m_selectedClassIndex = static_cast<int>(i);
                    m_selectedClassName = cls;
                    PopulateMemberList(cls);
                    // reset member UI state
                    m_selectedMemberIndex = -1;
                    m_editWidgetType = EditorWidgetType::Default;
                    m_editHasConfig = false;
                    m_editConfig = WidgetConfig();
                }
            }
        }

        ImGui::EndChild();

        // Right panel: list root members for selected class and show/edit mapping for selected member
        ImGui::SameLine();
        ImGui::BeginChild("##right_panel", ImVec2(0, 0), true);
        if (m_selectedClassIndex >= 0) {
            ImGui::Text("Selected class: %s", m_selectedClassName.c_str());
            ImGui::Separator();

            if (m_memberNames.empty()) {
                ImGui::TextWrapped("No root members found for this class.");
            }
            else {
                // Two-column layout: left - member list; right - details/edit
                ImGui::BeginChild("##members_col", ImVec2(300, 0), true);
                for (size_t i = 0; i < m_memberNames.size(); ++i) {
                    bool sel = (static_cast<int>(i) == m_selectedMemberIndex);
                    if (ImGui::Selectable(m_memberNames[i].c_str(), sel)) {
                        m_selectedMemberIndex = static_cast<int>(i);
                        LoadSelectedMemberMapping();
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("##member_details", ImVec2(0, 0), false);
                if (m_selectedMemberIndex < 0) {
                    ImGui::TextWrapped("Select a member to view/edit its widget mapping.");
                }
                else {
                    const std::string& memberName = m_memberNames[m_selectedMemberIndex];
                    ImGui::Text("Member: %s", memberName.c_str());
                    ImGui::Separator();

                    // Widget type combo
                    int typeIdx = static_cast<int>(m_editWidgetType);
                    if (ImGui::Combo("Widget Type", &typeIdx, s_WidgetTypeNames, IM_ARRAYSIZE(s_WidgetTypeNames))) {
                        m_editWidgetType = static_cast<EditorWidgetType>(typeIdx);
                        // If switching to a type that doesn't use config, clear config flag
                        if (m_editWidgetType != EditorWidgetType::Slider &&
                            m_editWidgetType != EditorWidgetType::Dropdown &&
                            m_editWidgetType != EditorWidgetType::FilePicker) {
                            m_editHasConfig = false;
                        }
                    }

                    // Config toggle for types that support config
                    if (m_editWidgetType == EditorWidgetType::Slider ||
                        m_editWidgetType == EditorWidgetType::Dropdown ||
                        m_editWidgetType == EditorWidgetType::FilePicker) {
                        ImGui::Checkbox("Has Config", &m_editHasConfig);
                        if (m_editHasConfig) {
                            ImGui::Separator();
                            // Slider config
                            if (m_editWidgetType == EditorWidgetType::Slider) {
                                ImGui::InputFloat("Min", &m_editConfig.minValue, 0.0f, 0.0f, "%.3f");
                                ImGui::InputFloat("Max", &m_editConfig.maxValue, 0.0f, 0.0f, "%.3f");
                                ImGui::InputFloat("Step", &m_editConfig.step, 0.0f, 0.0f, "%.3f");
                            }
                            // Dropdown options
                            if (m_editWidgetType == EditorWidgetType::Dropdown) {
                                // editable list of options - simple text area for comma-separated options
                                static char optionsBuf[1024] = { 0 };
                                // populate when first opened
                                if (optionsBuf[0] == '\0' && !m_editConfig.dropdownOptions.empty()) {
                                    std::string joined;
                                    for (size_t i = 0; i < m_editConfig.dropdownOptions.size(); ++i) {
                                        if (i) joined += ",";
                                        joined += m_editConfig.dropdownOptions[i];
                                    }
                                    strncpy(optionsBuf, joined.c_str(), sizeof(optionsBuf) - 1);
                                    optionsBuf[sizeof(optionsBuf) - 1] = '\0';
                                }
                                ImGui::InputTextMultiline("Options (comma separated)", optionsBuf, sizeof(optionsBuf), ImVec2(-1, 80));
                                if (ImGui::Button("Apply Options")) {
                                    m_editConfig.dropdownOptions.clear();
                                    std::string tmp(optionsBuf);
                                    size_t start = 0;
                                    while (true) {
                                        size_t pos = tmp.find(',', start);
                                        std::string token;
                                        if (pos == std::string::npos) {
                                            token = tmp.substr(start);
                                            if (!token.empty()) m_editConfig.dropdownOptions.push_back(token);
                                            break;
                                        }
                                        else {
                                            token = tmp.substr(start, pos - start);
                                            if (!token.empty()) m_editConfig.dropdownOptions.push_back(token);
                                            start = pos + 1;
                                        }
                                    }
                                }
                            }

                            // File picker filter
                            if (m_editWidgetType == EditorWidgetType::FilePicker) {
                                char buf[256];
                                strncpy(buf, m_editConfig.fileFilter.c_str(), sizeof(buf) - 1);
                                buf[sizeof(buf) - 1] = '\0';
                                if (ImGui::InputText("File Filter", buf, sizeof(buf))) {
                                    m_editConfig.fileFilter = std::string(buf);
                                }
                            }
                        }
                    }

                    ImGui::Separator();
                    if (ImGui::Button("Apply")) {
                        ApplySelectedMemberMapping();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Clear Mapping")) {
                        // Remove custom mapping (equivalent to default behavior)
                        auto map = PropertyWidgetMapRegistry::Instance().Get(m_selectedClassName);
                        if (map) {
                            map->RemoveWidget(memberName);
                            // re-register to update the serialized clone
                            PropertyWidgetMapRegistry::Instance().Register(m_selectedClassName, map);
                        }
                        // reset UI state
                        m_editWidgetType = EditorWidgetType::Default;
                        m_editHasConfig = false;
                        m_editConfig = WidgetConfig();
                    }
                }
                ImGui::EndChild();
            }
        }
        else {
            ImGui::TextWrapped("Select a class from the left to view its root members.");
        }
        ImGui::EndChild();

        ImGui::End();
        return true;
    }

} // namespace ImGuiVisualizers