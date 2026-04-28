#include "CoreSystem/CoreSystem.h"
#include "Net/NexusClient.h"
#include "..\SharedNexusDefines.h"

#include "FbxToBgfxMesh.h"
#include "CommandConsole.h"

#include "imgui.h"

// Ensure bx types before dialog.h
#include <bx/filepath.h>
#include <bx/string.h>
#include "..\\..\\External\\bgfx\\examples\\common\\entry\\dialog.h"

#include <thread>
#include <fstream>
#include <vector>
#include <cstring>
#include <filesystem>

bool FbxToBgfxMesh::Initialize()
{
	// Initialize application-specific resources here
	NEXUS_CONNECT_AND_REGISTER("127.0.0.1", 9500, "FbxToBgfxMesh", "nhill");
	m_visualizerManager.Initialize();
	m_visualizerManager.Register("Command Console", std::make_unique<CommandConsole>(), true);
	Core::CoreSystem::GetNexusClient()->EnableAutoReconnect();

	// Default hints
	std::strncpy(m_targetPath, ".", sizeof(m_targetPath) - 1);
	std::strncpy(m_sourcePath, "", sizeof(m_sourcePath) - 1);

	return true;
}

void FbxToBgfxMesh::Update(double deltaTime)
{
	// No runtime behaviour required for this tool app.
}

void FbxToBgfxMesh::Render(double deltaTime)
{

}

void FbxToBgfxMesh::ImguiUpdate()
{
	// Render registered visualizers first
	m_visualizerManager.RenderAll();

	// Simple converter UI
	if (ImGui::Begin("FBX/OBJ -> BGFX Converter"))
	{
		ImGui::TextWrapped("Choose input type, source file, and output directory. "
			"Toggle vertex attribute extraction and press Convert.");

		ImGui::Separator();

		// Input type
		const char* items[] = { "FBX", "OBJ" };
		int current = static_cast<int>(m_inputType);
		if (ImGui::Combo("Input Type", &current, items, IM_ARRAYSIZE(items)))
		{
			m_inputType = static_cast<InputType>(current);
		}

		ImGui::Separator();

		// Source selection
		ImGui::InputText("Source File", m_sourcePath, sizeof(m_sourcePath));
		ImGui::SameLine();
		if (ImGui::Button("Browse...##source"))
		{
			bx::FilePath fp(m_sourcePath);
			bx::StringView title("Select source file");
			// filter depends on input type
			bx::StringView filter = (m_inputType == InputType::FBX)
				? bx::StringView("FBX Files | *.fbx\nAll Files | *")
				: bx::StringView("OBJ Files | *.obj\nAll Files | *");

			if (openFileSelectionDialog(fp, FileSelectionDialogType::Open, title, filter))
			{
				const char* sel = fp.getCPtr();
				if (sel) std::strncpy(m_sourcePath, sel, sizeof(m_sourcePath) - 1);
			}
		}

		// Output directory selection
		ImGui::InputText("Output Directory", m_targetPath, sizeof(m_targetPath));
		ImGui::SameLine();
		if (ImGui::Button("Browse...##output"))
		{
			std::string initial = std::string(m_targetPath);
			if (initial.empty()) initial = ".";

			bx::FilePath fp(initial.c_str());
			bx::StringView title("Select output directory (choose or type filename)");
			bx::StringView filter("All Files | *");
			if (openFileSelectionDialog(fp, FileSelectionDialogType::Save, title, filter))
			{
				const char* sel = fp.getCPtr();
				if (sel)
				{
					try
					{
						std::filesystem::path p(sel);
						std::filesystem::path dir = p;
						if (std::filesystem::is_regular_file(p) || p.has_filename())
							dir = p.parent_path();

						std::string dirStr = dir.empty() ? "." : dir.string();
						std::strncpy(m_targetPath, dirStr.c_str(), sizeof(m_targetPath) - 1);
					}
					catch (...)
					{
						std::strncpy(m_targetPath, sel, sizeof(m_targetPath) - 1);
					}
				}
			}
		}

		ImGui::Separator();

		// Output format selection (affects extension only for now)
		const char* outItems[] = { "BGFX (.bgfx)", "BIN (.bin)" };
		int outCurrent = static_cast<int>(m_outputFormat);
		if (ImGui::Combo("Output Format", &outCurrent, outItems, IM_ARRAYSIZE(outItems)))
			m_outputFormat = static_cast<OutputFormat>(outCurrent);

		ImGui::Separator();

		// Conversion options
		ImGui::Checkbox("Include Normals", &m_convertOptions.includeNormals);
		ImGui::Checkbox("Include UVs", &m_convertOptions.includeUVs);
		if (m_convertOptions.includeUVs)
		{
			ImGui::Indent();
			ImGui::Checkbox("Generate Spherical UVs (if missing)", &m_convertOptions.generateSphericalUVs);
			if (m_convertOptions.generateSphericalUVs)
			{
				ImGui::InputFloat("Spherical UV Scale", &m_convertOptions.sphericalUVScale, 0.1f, 1.0f, "%.4f");
				if (m_convertOptions.sphericalUVScale <= 0.0f)
					m_convertOptions.sphericalUVScale = 1.0f;
				ImGui::TextDisabled("Maps the full sphere to [0, scale]. Use 1.0 for a single wrap.");
			}
			ImGui::Unindent();
		}
		ImGui::Checkbox("Include Tangents", &m_convertOptions.includeTangents);
		ImGui::InputFloat("Scale Factor", &m_convertOptions.scaleFactor, 0.1f, 1.0f, "%.4f");

		ImGui::Separator();

		bool disableConvert = m_isConverting.load();
		if (disableConvert) ImGui::BeginDisabled();

		if (ImGui::Button("Convert"))
		{
			// Basic validation
			if (std::strlen(m_sourcePath) == 0)
			{
				std::lock_guard<std::mutex> lock(m_statusMutex);
				m_statusMessage = "Source path is empty.";
			}
			else if (std::strlen(m_targetPath) == 0)
			{
				std::lock_guard<std::mutex> lock(m_statusMutex);
				m_statusMessage = "Target directory is empty.";
			}
			else
			{
				m_isConverting.store(true);
				{
					std::lock_guard<std::mutex> lock(m_statusMutex);
					m_statusMessage = "Conversion started...";
				}

				// copy state
				std::string srcPath(m_sourcePath);
				std::string outDir(m_targetPath);
				FbxConvertOptions opts = m_convertOptions;
				InputType inputType = m_inputType;
				OutputFormat outFmt = m_outputFormat;

				std::thread([this, srcPath = std::move(srcPath),
					outDir = std::move(outDir),
					opts = std::move(opts),
					inputType, outFmt]() mutable
					{
						std::vector<uint8_t> outBinary;
						bool ok = false;
						if (inputType == InputType::FBX)
						{
							ok = ConvertFbxToBgfxBinary(srcPath.c_str(), opts, outBinary);
						}
						else // OBJ
						{
							ok = ObjMeshConverter::ConvertObjToBgfxBinary(srcPath.c_str(), opts, outBinary);
						}

						if (!ok || outBinary.empty())
						{
							std::lock_guard<std::mutex> lock(m_statusMutex);
							m_statusMessage = "Conversion failed or produced no output.";
							m_isConverting.store(false);
							return;
						}

						// Build output file path from source stem and selected extension
						std::string outFile;
						try
						{
							std::filesystem::path src(srcPath);
							std::string base = src.stem().string();
							std::string ext = (outFmt == OutputFormat::BGFX) ? ".bgfx" : ".bin";
							std::filesystem::path outPath = std::filesystem::path(outDir) / (base + ext);
							outFile = outPath.string();
							if (!std::filesystem::exists(outPath.parent_path()))
								std::filesystem::create_directories(outPath.parent_path());
						}
						catch (...)
						{
							std::lock_guard<std::mutex> lock(m_statusMutex);
							m_statusMessage = "Failed to build output path.";
							m_isConverting.store(false);
							return;
						}

						// Write file
						std::ofstream ofs(outFile, std::ios::binary);
						if (!ofs)
						{
							std::lock_guard<std::mutex> lock(m_statusMutex);
							m_statusMessage = "Failed to open target file for writing: " + outFile;
							m_isConverting.store(false);
							return;
						}
						ofs.write(reinterpret_cast<const char*>(outBinary.data()), static_cast<std::streamsize>(outBinary.size()));
						ofs.close();

						std::lock_guard<std::mutex> lock(m_statusMutex);
						m_statusMessage = ok ? ("Conversion completed successfully: " + outFile) : "Conversion failed while writing output.";
						m_isConverting.store(false);
					}).detach();
			}
		}

		if (disableConvert) ImGui::EndDisabled();

		if (m_isConverting.load())
		{
			ImGui::SameLine();
			ImGui::TextUnformatted("Converting...");
		}

		ImGui::Separator();

		{
			std::lock_guard<std::mutex> lock(m_statusMutex);
			if (!m_statusMessage.empty())
				ImGui::TextWrapped("%s", m_statusMessage.c_str());
			else
				ImGui::TextDisabled("Status: idle");
		}
	}
	ImGui::End();
}

void FbxToBgfxMesh::ImguiMainMenu()
{
	m_visualizerManager.RenderMenuBar();
}

bool FbxToBgfxMesh::Shutdown()
{
	// Detached worker threads are allowed to finish on their own.
	return true;
}