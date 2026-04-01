#include "QuickEdit.h"
#include "CommandConsole.h"
#include "AssetBrowser.h"
#include "ObjJsonEditor.h"

#include "EntityDefinition.h"
#include "StaticMeshDefinition.h"

bool QuickEditApp::Initialize()
{
	// Initialize application-specific resources here
	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), true);

	// Register the generic object editor
	auto objEditor = std::make_unique<ImGuiVisualizers::ObjJsonEditor>();
	auto* objEditorPtr = objEditor.get();
	m_visualizerManager.Register("Object Editor", std::move(objEditor), false);

	// Asset Browser with example asset type registrations
	auto assetBrowser = std::make_unique<ImGuiVisualizers::AssetBrowser>();
	auto& registry = assetBrowser->GetRegistry();

	// Register asset types by multi-part extension
	registry.Register(".def.obj.json", "Definition Object", IM_COL32(100, 200, 255, 255), "O");
	registry.Register(".png", "Texture", IM_COL32(200, 150, 255, 255), "T");
	registry.Register(".bmp", "Texture", IM_COL32(200, 150, 255, 255), "T");
	registry.Register(".dds", "Texture", IM_COL32(200, 150, 255, 255), "T");

	// ── Configure launch options per asset type ─────────────────────────

	// .obj.json files: use the generic ObjJsonEditor
	if (auto* objType = registry.FindMutable(".def.obj.json")) {
		// The reflected class name is embedded in the filename:
		// e.g. "CEntityDefinition.obj.json" -> className = "CEntityDefinition"
		// Alternatively, set a fixed class name per registered sub-extension.
		objType->primaryLaunch = {
			"Object Editor",
			ImGuiVisualizers::AssetLaunchMode::Internal,
			[this, objEditorPtr](const std::string& path) {
				// Derive the class name from the filename.
				// Convention: <ClassName>.obj.json
				std::string fileName = path;
				// Strip directory part
				auto lastSlash = fileName.find_last_of("/\\");
				if (lastSlash != std::string::npos) {
					fileName = fileName.substr(lastSlash + 1);
				}
				// Strip ".obj.json" to get the class name
				const std::string suffix = ".def.obj.json";
				std::string className = "CEntityDefinition";

				if (!className.empty()) {
					objEditorPtr->Open(path, className);
					m_visualizerManager.SetVisible("Object Editor", true);
				}
			}
		};
		objType->secondaryLaunches.push_back({
			"Open as JSON (External)",
			ImGuiVisualizers::AssetLaunchMode::External,
			nullptr,
			""  // empty = OS default for .json
			});
	}

	// Material files: internal material editor
	if (auto* mat = registry.FindMutable(".mat.json")) {
		mat->primaryLaunch = {
			"Material Editor",
			ImGuiVisualizers::AssetLaunchMode::Internal,
			[](const std::string& path) {
				// TODO: open the internal material editor
			}
		};
		mat->secondaryLaunches.push_back({
			"Open as JSON (External)",
			ImGuiVisualizers::AssetLaunchMode::External,
			nullptr,
			""
			});
	}

	// Textures: internal preview as primary, external editor as secondary
	for (const char* ext : { ".png", ".bmp", ".jpg", ".dds" }) {
		if (auto* tex = registry.FindMutable(ext)) {
			tex->primaryLaunch = {
				"Texture Preview",
				ImGuiVisualizers::AssetLaunchMode::Internal,
				[](const std::string& path) {
					// TODO: open the internal texture previewer
				}
			};
			tex->secondaryLaunches.push_back({
				"Open in External Editor",
				ImGuiVisualizers::AssetLaunchMode::External,
				nullptr,
				""
				});
		}
	}

	// Lua scripts: open externally by default
	if (auto* lua = registry.FindMutable(".lua")) {
		lua->primaryLaunch = {
			"Open in External Editor",
			ImGuiVisualizers::AssetLaunchMode::External,
			nullptr,
			""
		};
	}

	m_visualizerManager.Register("Asset Browser", std::move(assetBrowser), true);

	return true;
}

void QuickEditApp::Update(double deltaTime)
{
	m_visualizerManager.Update(static_cast<float>(deltaTime));
}

void QuickEditApp::Render(double deltaTime)
{

}

void QuickEditApp::ImguiUpdate()
{
	m_visualizerManager.RenderAll();
}

void QuickEditApp::ImguiMainMenu()
{
	m_visualizerManager.RenderMenuBar();
}

bool QuickEditApp::Shutdown()
{
	return true;
}
