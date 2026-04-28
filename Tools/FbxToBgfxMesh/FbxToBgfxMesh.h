#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <atomic>

#include "app\AppInterface.h"

#include "ImGuiVisualizerManager.h"
#include "FbxMeshConverter.h"
#include "ObjMeshConverter.h"

class FbxToBgfxMesh : public IApplication
{
public:
	bool Initialize() override;
	void Update(double deltaTime) override;
	void Render(double deltaTime) override;
	void ImguiUpdate() override;
	void ImguiMainMenu() override;
	bool Shutdown() override;

private:
	ImGuiVisualizers::ImGuiVisualizerManager m_visualizerManager;
	std::vector<float> m_pendingFPSUpdates;

	// UI state
	char m_sourcePath[1024] = {};
	char m_targetPath[1024] = {}; // will hold the output directory path

	// Choose input type
	enum class InputType { FBX = 0, OBJ = 1 };
	InputType m_inputType = InputType::OBJ;

	// Output format / extension selection (expandable)
	enum class OutputFormat { BGFX = 0, BIN = 1 };
	OutputFormat m_outputFormat = OutputFormat::BIN;

	// Conversion options exposed to the UI
	FbxConvertOptions m_convertOptions;

	// Conversion state / status
	std::atomic<bool> m_isConverting{ false };
	std::string        m_statusMessage;
	std::mutex         m_statusMutex;
};