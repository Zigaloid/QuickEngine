#pragma once

#include "IImGuiVisualizer.h"
#include "PropertyInspector.h"
#include "Reflection/ReflectionBase.h"
#include "ClassFactory/ClassFactory.h"

#include <string>
#include <memory>

namespace ImGuiVisualizers {

	/**
	 * @brief Generic editor for .obj.json files.
	 *
	 * Uses the ClassFactory reflection system to instantiate an object by class
	 * name, loads the JSON file via CReflectedBase::Read(), and presents it
	 * through an embedded PropertyInspector for viewing and editing.
	 *
	 * Multiple instances can coexist — each gets a unique ImGui window ID.
	 */
	class ObjJsonEditor : public IImGuiVisualizer
	{
	public:
		ObjJsonEditor()
			: m_instanceId(s_nextInstanceId++)
			, m_imguiId("###ObjJsonEditor_" + std::to_string(m_instanceId))
			, m_displayName("Object Editor")
		{}
		~ObjJsonEditor() override = default;

		// ── IImGuiVisualizer interface ──────────────────────────────────────

		void Initialize() override {}
		void Shutdown() override { Close(); }
		void Update(float deltaTime) override { (void)deltaTime; }

		bool Render(bool* isOpen) override
		{
			// Build window title with unique ImGui ID for multi-instance support
			std::string title;
			if (m_object) {
				title = "Object Editor - " + m_displayName + m_imguiId;
			} else {
				title = "Object Editor" + m_imguiId;
			}

			bool visible = ImGui::Begin(title.c_str(), isOpen);

			// Handle window close via X button
			if (isOpen && !*isOpen) {
				Close();
				ImGui::End();
				return false;
			}

			if (!visible) {
				ImGui::End();
				return false;
			}

			if (!m_object) {
				// Nothing loaded – render an empty window
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
					"No object loaded. Open an .obj.json file from the Asset Browser.");
				ImGui::End();
				return true;
			}

			static char* buffer = new char[200];
			buffer[0] = '\0';

			ImGui::InputText("TextTest", buffer, 200);
			
			// Toolbar
			if (ImGui::Button("Save")) {
				Save();
			}
			ImGui::SameLine();
			if (ImGui::Button("Reload")) {
				Reload();
			}
			ImGui::SameLine();
			if (ImGui::Button("Close")) {
				Close();
				if (isOpen) *isOpen = false;
				ImGui::End();
				return true;
			}

			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f),
				"[%s]", m_className.c_str());

			if (!m_statusMessage.empty()) {
				ImVec4 statusColor = m_statusIsError
					? ImVec4(1.0f, 0.3f, 0.3f, 1.0f)
					: ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
				ImGui::SameLine();
				ImGui::TextColored(statusColor, "%s", m_statusMessage.c_str());
			}

			ImGui::Separator();

			// Delegate to the embedded PropertyInspector (inline, no extra window)
			m_inspector.RenderContent();

			ImGui::End();
			return true;
		}

		const char* GetName() const override { return m_displayName.c_str(); }
		const char* GetShortcut() const override { return nullptr; }
		const char* GetMenuCategory() const override { return "Show"; }

		// ── Public API ──────────────────────────────────────────────────────

		/**
		 * @brief Open a .obj.json file using the reflection system.
		 * @param filePath   Full path to the .obj.json file.
		 * @param className  Reflected class name to instantiate via ClassFactory
		 *                   (e.g. "CEntityDefinition").
		 * @return true if the object was created and loaded successfully.
		 */
		bool Open(const std::string& filePath, const std::string& className)
		{
			Close();

			m_filePath = filePath;
			m_className = className;

			// Update display name from filename
			std::string fileName = filePath;
			auto lastSlash = fileName.find_last_of("/\\");
			if (lastSlash != std::string::npos) {
				fileName = fileName.substr(lastSlash + 1);
			}
			m_displayName = fileName;

			// Instantiate via the reflection class factory
			CReflectedBase* raw = ClassFactory::createObject(className.c_str());
			if (!raw) {
				m_statusMessage = "ClassFactory: unknown class '" + className + "'";
				m_statusIsError = true;
				return false;
			}

			m_object.reset(raw);

			// Load the JSON file into the object
			if (!m_object->Read(filePath.c_str())) {
				m_statusMessage = "Failed to read '" + filePath + "'";
				m_statusIsError = true;
				// Keep the default-constructed object so properties are still visible
			}
			else {
				m_statusMessage = "Loaded";
				m_statusIsError = false;
			}

			m_inspector.SetObject(m_object.get());
			return true;
		}

		/**
		 * @brief Save the current object back to the file it was loaded from.
		 */
		bool Save()
		{
			if (!m_object || m_filePath.empty()) {
				return false;
			}

			if (m_object->Write(m_filePath.c_str())) {
				m_statusMessage = "Saved";
				m_statusIsError = false;
				return true;
			}

			m_statusMessage = "Failed to save '" + m_filePath + "'";
			m_statusIsError = true;
			return false;
		}

		/**
		 * @brief Reload the file from disk, discarding in-memory changes.
		 */
		bool Reload()
		{
			if (m_className.empty() || m_filePath.empty()) {
				return false;
			}
			return Open(m_filePath, m_className);
		}

		/**
		 * @brief Close the current object and free resources.
		 */
		void Close()
		{
			m_inspector.ClearObject();
			m_object.reset();
			m_filePath.clear();
			m_className.clear();
			m_statusMessage.clear();
			m_statusIsError = false;
			m_displayName = "Object Editor";
		}

		/**
		 * @brief Check whether an object is currently loaded.
		 */
		bool IsLoaded() const { return m_object != nullptr; }

		/**
		 * @brief Get the path of the currently loaded file.
		 */
		const std::string& GetFilePath() const { return m_filePath; }

		/**
		 * @brief Generate a unique visualizer-manager key for a document path.
		 *
		 * Use this when registering / looking up per-document editor instances
		 * in the ImGuiVisualizerManager.
		 */
		static std::string MakeDocumentKey(const std::string& filePath)
		{
			return "ObjEditor:" + filePath;
		}

	private:
		static inline int s_nextInstanceId = 0;
		int               m_instanceId = 0;
		std::string       m_imguiId;          ///< "###ObjJsonEditor_<N>" for unique window IDs
		std::string       m_displayName;      ///< Shown in menus / window title

		std::unique_ptr<CReflectedBase> m_object;
		PropertyInspector               m_inspector;

		std::string m_filePath;
		std::string m_className;
		std::string m_statusMessage;
		bool        m_statusIsError = false;
	};

} // namespace ImGuiVisualizers