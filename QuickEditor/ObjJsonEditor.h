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
	 */
	class ObjJsonEditor : public IImGuiVisualizer
	{
	public:
		ObjJsonEditor() = default;
		~ObjJsonEditor() override = default;

		// ── IImGuiVisualizer interface ──────────────────────────────────────

		void Initialize() override {}
		void Shutdown() override { Close(); }
		void Update(float deltaTime) override { (void)deltaTime; }

		bool Render(bool* isOpen) override
		{
			if (!m_object) {
				// Nothing loaded – render an empty window
				if (!ImGui::Begin("Object Editor", isOpen)) {
					ImGui::End();
					return false;
				}
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
					"No object loaded. Open an .obj.json file from the Asset Browser.");
				ImGui::End();
				return true;
			}

			// Build a descriptive window title
			std::string title = "Object Editor - " + m_filePath + "###ObjJsonEditor";
			if (!ImGui::Begin(title.c_str(), isOpen)) {
				ImGui::End();
				return false;
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

		const char* GetName() const override { return "Object Editor"; }
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
		}

		/**
		 * @brief Check whether an object is currently loaded.
		 */
		bool IsLoaded() const { return m_object != nullptr; }

		/**
		 * @brief Get the path of the currently loaded file.
		 */
		const std::string& GetFilePath() const { return m_filePath; }

	private:
		std::unique_ptr<CReflectedBase> m_object;
		PropertyInspector               m_inspector;

		std::string m_filePath;
		std::string m_className;
		std::string m_statusMessage;
		bool        m_statusIsError = false;
	};

} // namespace ImGuiVisualizers