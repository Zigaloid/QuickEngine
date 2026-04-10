#pragma once

#include "IImGuiVisualizer.h"
#include "ImGui3DViewVisualizer.h"
#include "ObjJsonEditor.h"
#include "MessageSystem/MessageBus.h"

#include "imgui/imgui.h"
#include <string>

namespace ImGuiVisualizers {

class CombinedObjJson3DVisualizer : public IImGuiVisualizer {
public:
    CombinedObjJson3DVisualizer(const char* name = "Mesh Editor")
        : m_windowName(name)
        , m_view("3D View (Obj)", nullptr, "Visualizers")
    {}

    ~CombinedObjJson3DVisualizer() override = default;

    void Initialize() override
    {
        m_editor.RegisterEditorActions();
        m_view.Initialize();
        // Subscribe using the Message type and extract the string payload.
        Core::MessageSystem::MessageBus::Get().Subscribe("ObjJsonEditor.FileSaved",
            [this](const Core::MessageSystem::Message& msg) 
            {
                if (!msg.HasString()) return;
                if (m_fileName == msg.str)
                {
                    const std::string& filePath = msg.str;
                    if (filePath == m_editor.GetFilePath()) {
                        m_view.LoadMesh(m_editor.GetFilePath());
                    }
                }
            });
    }

    void Shutdown() override
    {
        m_view.Shutdown();
        m_editor.Close();
    }

    void Update(float deltaTime) override
    {
        m_view.Update(deltaTime);
    }
    static std::string MakeDocumentKey(const std::string& filePath)
    {
        return "MeshComponentEditor:" + filePath;
    }

    bool Render(bool* isOpen) override
    {
        // Build window title with unique ImGui ID for multi-instance support
        std::string title;
        title = "Mesh Editor- " + m_fileName;

        if (!ImGui::Begin(title.c_str(), isOpen, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::End();
            return false;
        }
        
        // Render inspector inline
        if (ImGui::BeginMenuBar())
        {
            m_editor.GetActionManager().RenderMenuBar();
            ImGui::EndMenuBar();
        }
        // Toolbar (render via UnifiedActionManager)
        m_editor.GetActionManager().RenderToolbar();
        // Layout: left = 3D view (approx 65%), right = inspector (35%)
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float leftW = avail.x * 0.65f;
        float rightW = avail.x - leftW;

        // Left pane (3D)
        ImGui::BeginChild("##Left3D", ImVec2(leftW, 0), false);
        // Render the 3D view content into this child (toolbar + viewport)
        ImVec2 leftAvail = ImGui::GetContentRegionAvail();
        if (leftAvail.x < 1.0f) leftAvail.x = 1.0f;
        if (leftAvail.y < 1.0f) leftAvail.y = 1.0f;
       
        m_view.RenderContent(leftAvail);
        ImGui::EndChild();

        ImGui::SameLine();

        // Right pane (inspector)
        ImGui::BeginChild("##RightInspector", ImVec2(rightW, 0), false);
        if (!m_editor.IsLoaded()) {
            ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1.0f),
                "No object loaded. Use the Asset Browser to open an .obj.json file.");
        } else {

            m_editor.RenderInspectorInline();
        }
        ImGui::EndChild();

        ImGui::End();
        return true;
    }

    const char* GetName() const override { return m_windowName.c_str(); }
    const char* GetShortcut() const override { return nullptr; }
    const char* GetMenuCategory() const override { return "Show"; }

    // Forward helper operations to the embedded editor
    bool OpenObjectFile(const std::string& filePath, const std::string& className)
    {
        m_fileName = filePath;
        return m_editor.Open(filePath, className);
    }
    bool SaveObjectFile() { return m_editor.Save(); }
    void CloseObject() { m_editor.Close(); }

    ImGui3DViewVisualizer& Get3DView() { return m_view; }
    ObjJsonEditor& GetEditor() { return m_editor; }

private:
    std::string m_fileName;
    std::string m_windowName;
    ImGui3DViewVisualizer m_view;
    ObjJsonEditor         m_editor;
};

} // namespace ImGuiVisualizers