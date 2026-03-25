#include "GLTFResource.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <set>
#include <cmath>

namespace ResourceSystem {

	bool GLTFResource::Initialize() {
		if (!Resource::Initialize()) {
			return false;
		}
		return true;
	}

	bool GLTFResource::Update(FileSystem::FileSystemManager& fileSystem) {
		DECLARE_FUNC_LOW();
		if (isLoaded_) {
			return true;
		}

		// Load file data using FileSystemManager
		auto result = fileSystem.ReadAllBytes(path_);
		if (!result.IsSuccess()) {
			std::cerr << "Failed to load GLTF file: " << path_ << std::endl;
			return false;
		}

		data_ = result.GetValue();

		// Parse JSON
		if (!parseJSON()) {
			std::cerr << "Failed to parse GLTF JSON" << std::endl;
			return false;
		}

		// Parse all components in correct order
		if (!parseBuffers(fileSystem) ||
			!parseBufferViews() ||
			!parseAccessors() ||
			!parseMaterials() ||
			!parseMeshes() ||
			!parseNodes() ||
			!parseScenes()) {
			std::cerr << "Failed to parse GLTF components" << std::endl;
			return false;
		}

		// Generate convex hull for the entire resource.
		generateConvexHull();

		isLoaded_ = true;
		return true;
	}

	void GLTFResource::Finalize() {
		DECLARE_FUNC_LOW();
		if (isFinalized_ || !isLoaded_) {
			Resource::Finalize();
			return;
		}

		// Create OpenGL buffers for all meshes
		if (!createOpenGLBuffers()) {
			std::cerr << "Failed to create OpenGL buffers" << std::endl;
			Resource::Finalize();
			return;
		}

		// Load textures (if resource manager is available)
		if (m_resourceManager) {
			loadTextures();
		}

		// Compute node transforms
		computeNodeTransforms();

		Resource::Finalize();
	}

	void GLTFResource::generateConvexHull() {
		std::cout << "GLTFResource: Generating convex hull for entire resource..." << std::endl;

		// Generate resource-level convex hull by combining all vertices from all meshes
		std::vector<float> allResourceVertices;
		for (const auto& mesh : m_meshes) {
			for (const auto& primitive : mesh.primitives) {
				const auto& positions = primitive.vertexData.positions;
				allResourceVertices.insert(allResourceVertices.end(), positions.begin(), positions.end());
			}
		}

		if (!allResourceVertices.empty()) {
			m_resourceConvexHull = Geometry::ConvexHullGenerator::GenerateFromVertices(allResourceVertices);
			if (m_resourceConvexHull) {
				std::cout << "  Generated resource-level convex hull with "
					<< allResourceVertices.size() / 3 << " total vertices from "
					<< m_meshes.size() << " meshes" << std::endl;
			}
			else {
				std::cout << "  Failed to generate resource-level convex hull" << std::endl;
			}
		}
		else {
			std::cout << "  No vertices found for resource-level convex hull" << std::endl;
		}

		std::cout << "GLTFResource: Convex hull generation completed" << std::endl;
	}

	bool GLTFResource::parseJSON() {
		try {
			std::string jsonString(data_.begin(), data_.end());

			// Use RapidJSON to parse the JSON string
			rapidjson::ParseResult result = m_document.Parse(jsonString.c_str());
			if (!result) {
				std::cerr << "JSON parse error at offset " << result.Offset() << std::endl;
				return false;
			}

			if (!m_document.IsObject()) {
				std::cerr << "Root JSON element is not an object" << std::endl;
				return false;
			}

			return true;
		}
		catch (const std::exception& e) {
			std::cerr << "Exception parsing GLTF JSON: " << e.what() << std::endl;
			return false;
		}
	}

	bool GLTFResource::parseBuffers(FileSystem::FileSystemManager& fileSystem) {
		if (!m_document.HasMember("buffers")) {
			return true; // No buffers is valid
		}

		const rapidjson::Value& buffersArray = m_document["buffers"];
		if (!buffersArray.IsArray()) {
			std::cerr << "buffers is not an array" << std::endl;
			return false;
		}

		m_buffers.resize(buffersArray.Size());

		for (rapidjson::SizeType i = 0; i < buffersArray.Size(); ++i) {
			const rapidjson::Value& buffer = buffersArray[i];

			if (buffer.HasMember("uri")) {
				// External buffer - load using FileSystemManager
				std::string uri = buffer["uri"].GetString();

				// Check if it's a data URI (embedded base64)
				if (uri.find("data:") == 0) {
					// Handle data URI
					if (!parseDataURI(uri, m_buffers[i])) {
						std::cerr << "Failed to parse data URI for buffer " << i << std::endl;
						return false;
					}
				}
				else {
					// External file - resolve relative to GLTF file
					std::string bufferPath = resolveRelativePath(uri);

					auto result = fileSystem.ReadAllBytes(bufferPath);
					if (!result.IsSuccess()) {
						std::cerr << "Failed to load external buffer: " << bufferPath << std::endl;
						return false;
					}

					m_buffers[i] = result.GetValue();
				}
			}
			else {
				// GLB embedded buffer - extract from binary chunk
				if (!extractGLBBuffer(i, m_buffers[i])) {
					std::cerr << "Failed to extract GLB buffer " << i << std::endl;
					return false;
				}
			}

			// Validate buffer size
			if (buffer.HasMember("byteLength")) {
				size_t expectedSize = buffer["byteLength"].GetUint64();
				if (m_buffers[i].size() != expectedSize) {
					std::cerr << "Buffer " << i << " size mismatch. Expected: " << expectedSize
						<< ", Got: " << m_buffers[i].size() << std::endl;
					return false;
				}
			}
		}

		return true;
	}

	bool GLTFResource::parseDataURI(const std::string& uri, std::vector<uint8_t>& output) {
		// Parse data URI format: data:[<mediatype>][;base64],<data>
		const std::string prefix = "data:";
		if (uri.find(prefix) != 0) {
			return false;
		}

		size_t commaPos = uri.find(',');
		if (commaPos == std::string::npos) {
			return false;
		}

		std::string header = uri.substr(prefix.length(), commaPos - prefix.length());
		std::string data = uri.substr(commaPos + 1);

		// Check if it's base64 encoded
		if (header.find("base64") != std::string::npos) {
			return decodeBase64(data, output);
		}
		else {
			// URL encoded data - convert to bytes
			output.assign(data.begin(), data.end());
			return true;
		}
	}

	bool GLTFResource::decodeBase64(const std::string& encoded, std::vector<uint8_t>& output) {
		// Simple base64 decoder
		const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		std::vector<int> decode(256, -1);
		for (int i = 0; i < 64; ++i) {
			decode[chars[i]] = i;
		}

		output.clear();
		output.reserve((encoded.length() * 3) / 4);

		for (size_t i = 0; i < encoded.length();) {
			uint32_t tmp = 0;
			int padding = 0;

			for (int j = 0; j < 4 && i < encoded.length(); ++j, ++i) {
				if (encoded[i] == '=') {
					padding++;
				}
				else {
					int val = decode[static_cast<unsigned char>(encoded[i])];
					if (val == -1) {
						return false; // Invalid character
					}
					tmp = (tmp << 6) | val;
				}
			}

			for (int j = 0; j < 3 - padding; ++j) {
				output.push_back((tmp >> (8 * (2 - j))) & 0xFF);
			}
		}

		return true;
	}

	bool GLTFResource::extractGLBBuffer(size_t bufferIndex, std::vector<uint8_t>& output) {
		// For GLB format, the buffer data is embedded in the binary chunk
		// This is a simplified implementation - in a full implementation,
		// you would need to parse the GLB header and extract the binary chunk

		// For now, we'll assume the data after JSON is the buffer
		// This needs proper GLB parsing in production code
		std::cerr << "GLB buffer extraction not fully implemented" << std::endl;
		return false;
	}

	std::string GLTFResource::resolveRelativePath(const std::string& relativePath) {
		// Get the directory of the GLTF file
		std::filesystem::path gltfPath(path_);
		std::filesystem::path directory = gltfPath.parent_path();

		// Combine with relative path
		std::filesystem::path fullPath = directory / relativePath;

		return fullPath.string();
	}

	bool GLTFResource::parseMaterials() {
		if (!m_document.HasMember("materials")) {
			// Create default material
			GLTFMaterial defaultMaterial;
			defaultMaterial.name = "default";
			m_materials.push_back(defaultMaterial);
			return true;
		}

		const rapidjson::Value& materialsArray = m_document["materials"];
		if (!materialsArray.IsArray()) {
			std::cerr << "materials is not an array" << std::endl;
			return false;
		}

		m_materials.reserve(materialsArray.Size());

		for (rapidjson::SizeType i = 0; i < materialsArray.Size(); ++i) {
			const rapidjson::Value& material = materialsArray[i];
			GLTFMaterial gltfMaterial;

			if (material.HasMember("name") && material["name"].IsString()) {
				gltfMaterial.name = material["name"].GetString();
			}

			// Parse PBR metallic roughness
			if (material.HasMember("pbrMetallicRoughness") && material["pbrMetallicRoughness"].IsObject()) {
				const rapidjson::Value& pbr = material["pbrMetallicRoughness"];

				if (pbr.HasMember("baseColorFactor") && pbr["baseColorFactor"].IsArray()) {
					const rapidjson::Value& factor = pbr["baseColorFactor"];
					if (factor.Size() >= 3) {
						gltfMaterial.baseColorFactor = Vector3f(
							factor[0].GetFloat(),
							factor[1].GetFloat(),
							factor[2].GetFloat()
						);
					}
				}

				if (pbr.HasMember("metallicFactor") && pbr["metallicFactor"].IsNumber()) {
					gltfMaterial.metallicFactor = pbr["metallicFactor"].GetFloat();
				}

				if (pbr.HasMember("roughnessFactor") && pbr["roughnessFactor"].IsNumber()) {
					gltfMaterial.roughnessFactor = pbr["roughnessFactor"].GetFloat();
				}

				// Parse base color texture
				if (pbr.HasMember("baseColorTexture") && pbr["baseColorTexture"].IsObject()) {
					const rapidjson::Value& texture = pbr["baseColorTexture"];
					if (texture.HasMember("index") && texture["index"].IsInt()) {
						int textureIndex = texture["index"].GetInt();
						// Store texture index for later loading
						// The actual texture loading happens in loadTextures()
					}
				}
			}

			// Parse normal texture
			if (material.HasMember("normalTexture") && material["normalTexture"].IsObject()) {
				const rapidjson::Value& texture = material["normalTexture"];
				if (texture.HasMember("index") && texture["index"].IsInt()) {
					int textureIndex = texture["index"].GetInt();
					// Store for later loading
				}
			}

			// Parse emissive properties
			if (material.HasMember("emissiveFactor") && material["emissiveFactor"].IsArray()) {
				const rapidjson::Value& factor = material["emissiveFactor"];
				if (factor.Size() >= 3) {
					gltfMaterial.emissiveFactor = Vector3f(
						factor[0].GetFloat(),
						factor[1].GetFloat(),
						factor[2].GetFloat()
					);
				}
			}

			// Parse alpha mode
			if (material.HasMember("alphaMode") && material["alphaMode"].IsString()) {
				std::string alphaMode = material["alphaMode"].GetString();
				if (alphaMode == "OPAQUE") {
					gltfMaterial.alphaMode = GLTFMaterial::AlphaMode::OPAQUE;
				}
				else if (alphaMode == "MASK") {
					gltfMaterial.alphaMode = GLTFMaterial::AlphaMode::MASK;
				}
				else if (alphaMode == "BLEND") {
					gltfMaterial.alphaMode = GLTFMaterial::AlphaMode::BLEND;
				}
			}

			if (material.HasMember("alphaCutoff") && material["alphaCutoff"].IsNumber()) {
				gltfMaterial.alphaCutoff = material["alphaCutoff"].GetFloat();
			}

			if (material.HasMember("doubleSided") && material["doubleSided"].IsBool()) {
				gltfMaterial.doubleSided = material["doubleSided"].GetBool();
			}

			m_materials.push_back(gltfMaterial);
		}

		return true;
	}

	bool GLTFResource::parseMeshes() {
		if (!m_document.HasMember("meshes")) {
			return true;
		}

		const rapidjson::Value& meshesArray = m_document["meshes"];
		if (!meshesArray.IsArray()) {
			std::cerr << "meshes is not an array" << std::endl;
			return false;
		}

		m_meshes.reserve(meshesArray.Size());

		for (rapidjson::SizeType i = 0; i < meshesArray.Size(); ++i) {
			const rapidjson::Value& mesh = meshesArray[i];
			GLTFMesh gltfMesh;

			if (mesh.HasMember("name") && mesh["name"].IsString()) {
				gltfMesh.name = mesh["name"].GetString();
			}

			if (mesh.HasMember("primitives") && mesh["primitives"].IsArray()) {
				const rapidjson::Value& primitives = mesh["primitives"];
				gltfMesh.primitives.reserve(primitives.Size());

				for (rapidjson::SizeType j = 0; j < primitives.Size(); ++j) {
					const rapidjson::Value& primitive = primitives[j];
					GLTFPrimitive gltfPrimitive;

					// Parse mode
					if (primitive.HasMember("mode") && primitive["mode"].IsInt()) {
						int mode = primitive["mode"].GetInt();
						switch (mode) {
						case 0: gltfPrimitive.mode = GL_POINTS; break;
						case 1: gltfPrimitive.mode = GL_LINES; break;
						case 2: gltfPrimitive.mode = GL_LINE_LOOP; break;
						case 3: gltfPrimitive.mode = GL_LINE_STRIP; break;
						case 4: gltfPrimitive.mode = GL_TRIANGLES; break;
						case 5: gltfPrimitive.mode = GL_TRIANGLE_STRIP; break;
						case 6: gltfPrimitive.mode = GL_TRIANGLE_FAN; break;
						default: gltfPrimitive.mode = GL_TRIANGLES; break;
						}
					}

					// Parse material
					if (primitive.HasMember("material") && primitive["material"].IsInt()) {
						gltfPrimitive.materialIndex = primitive["material"].GetInt();
					}

					// Parse indices
					if (primitive.HasMember("indices") && primitive["indices"].IsInt()) {
						int accessorIndex = primitive["indices"].GetInt();
						if (!parseAccessorData(accessorIndex, gltfPrimitive)) {
							std::cerr << "Failed to parse indices for primitive " << j << std::endl;
							return false;
						}
					}

					// Parse attributes
					if (primitive.HasMember("attributes") && primitive["attributes"].IsObject()) {
						const rapidjson::Value& attributes = primitive["attributes"];
						if (!parseAttributeData(attributes, gltfPrimitive)) {
							std::cerr << "Failed to parse attributes for primitive " << j << std::endl;
							return false;
						}
					}

					gltfMesh.primitives.emplace_back(std::move(gltfPrimitive));
				}
			}

			m_meshes.emplace_back(std::move(gltfMesh));
		}

		return true;
	}

	bool GLTFResource::parseNodes() {
		if (!m_document.HasMember("nodes")) {
			return true;
		}

		const rapidjson::Value& nodesArray = m_document["nodes"];
		if (!nodesArray.IsArray()) {
			std::cerr << "nodes is not an array" << std::endl;
			return false;
		}

		m_nodes.reserve(nodesArray.Size());

		for (rapidjson::SizeType i = 0; i < nodesArray.Size(); ++i) {
			const rapidjson::Value& node = nodesArray[i];
			GLTFNode gltfNode;

			if (node.HasMember("name") && node["name"].IsString()) {
				gltfNode.name = node["name"].GetString();
			}

			// Parse transform
			if (node.HasMember("matrix") && node["matrix"].IsArray()) {
				const rapidjson::Value& matrix = node["matrix"];
				if (matrix.Size() == 16) {
					// GLTF matrices are column-major, same as Matrix4f
					std::array<float, 16> matrixData;
					for (rapidjson::SizeType j = 0; j < 16; ++j) {
						matrixData[j] = matrix[j].GetFloat();
					}
					gltfNode.matrix = Matrix4f(matrixData);
				}
			}
			else {
				// Parse TRS (Translation, Rotation, Scale)
				gltfNode.matrix.identity(); // Start with identity

				if (node.HasMember("translation") && node["translation"].IsArray()) {
					const rapidjson::Value& translation = node["translation"];
					if (translation.Size() >= 3) {
						gltfNode.translation = Vector3f(
							translation[0].GetFloat(),
							translation[1].GetFloat(),
							translation[2].GetFloat()
						);
					}
				}

				if (node.HasMember("scale") && node["scale"].IsArray()) {
					const rapidjson::Value& scale = node["scale"];
					if (scale.Size() >= 3) {
						gltfNode.scale = Vector3f(
							scale[0].GetFloat(),
							scale[1].GetFloat(),
							scale[2].GetFloat()
						);
					}
				}

				if (node.HasMember("rotation") && node["rotation"].IsArray()) {
					const rapidjson::Value& rotation = node["rotation"];
					if (rotation.Size() >= 4) {
						// Parse quaternion rotation (x, y, z, w)
						float x = rotation[0].GetFloat();
						float y = rotation[1].GetFloat();
						float z = rotation[2].GetFloat();
						float w = rotation[3].GetFloat();

						// TODO: Convert quaternion to matrix
						// For now, we'll skip rotation and just use TRS without R
					}
				}

				// Build matrix from TRS
				Matrix4f translationMatrix = Matrix4f::translation(gltfNode.translation.x, gltfNode.translation.y, gltfNode.translation.z);
				Matrix4f scaleMatrix = Matrix4f::scale(gltfNode.scale.x, gltfNode.scale.y, gltfNode.scale.z);
				gltfNode.matrix = translationMatrix * scaleMatrix;
			}

			// Parse children
			if (node.HasMember("children") && node["children"].IsArray()) {
				const rapidjson::Value& children = node["children"];
				for (rapidjson::SizeType j = 0; j < children.Size(); ++j) {
					if (children[j].IsInt()) {
						gltfNode.children.push_back(children[j].GetInt());
					}
				}
			}

			// Parse mesh
			if (node.HasMember("mesh") && node["mesh"].IsInt()) {
				gltfNode.meshIndex = node["mesh"].GetInt();
			}

			m_nodes.push_back(gltfNode);
		}

		// Set parent relationships
		for (size_t i = 0; i < m_nodes.size(); ++i) {
			for (int childIndex : m_nodes[i].children) {
				if (childIndex >= 0 && childIndex < static_cast<int>(m_nodes.size())) {
					m_nodes[childIndex].parent = static_cast<int>(i);
				}
			}
		}

		return true;
	}

	bool GLTFResource::parseScenes() {
		if (!m_document.HasMember("scenes")) {
			return true;
		}

		const rapidjson::Value& scenesArray = m_document["scenes"];
		if (!scenesArray.IsArray()) {
			std::cerr << "scenes is not an array" << std::endl;
			return false;
		}

		m_scenes.reserve(scenesArray.Size());

		for (rapidjson::SizeType i = 0; i < scenesArray.Size(); ++i) {
			const rapidjson::Value& scene = scenesArray[i];
			GLTFScene gltfScene;

			if (scene.HasMember("name") && scene["name"].IsString()) {
				gltfScene.name = scene["name"].GetString();
			}

			if (scene.HasMember("nodes") && scene["nodes"].IsArray()) {
				const rapidjson::Value& nodes = scene["nodes"];
				for (rapidjson::SizeType j = 0; j < nodes.Size(); ++j) {
					if (nodes[j].IsInt()) {
						gltfScene.rootNodes.push_back(nodes[j].GetInt());
					}
				}
			}

			m_scenes.push_back(gltfScene);
		}

		// Set default scene
		if (m_document.HasMember("scene") && m_document["scene"].IsInt()) {
			m_defaultScene = m_document["scene"].GetInt();
		}

		return true;
	}

	bool GLTFResource::loadTextures() {
		if (!m_resourceManager) {
			// No resource manager available - textures won't be loaded
			return true;
		}

		if (!m_document.HasMember("textures") || !m_document.HasMember("images")) {
			// No textures or images in the GLTF file
			return true;
		}

		try {
			const rapidjson::Value& texturesArray = m_document["textures"];
			const rapidjson::Value& imagesArray = m_document["images"];

			if (!texturesArray.IsArray() || !imagesArray.IsArray()) {
				std::cerr << "textures or images is not an array" << std::endl;
				return false;
			}

			// Create a mapping from texture index to image path
			std::unordered_map<int, std::string> textureToImagePath;

			// Parse textures array to map texture indices to image indices
			for (rapidjson::SizeType i = 0; i < texturesArray.Size(); ++i) {
				const rapidjson::Value& texture = texturesArray[i];

				if (texture.HasMember("source") && texture["source"].IsInt()) {
					int imageIndex = texture["source"].GetInt();

					if (imageIndex >= 0 && imageIndex < static_cast<int>(imagesArray.Size())) {
						const rapidjson::Value& image = imagesArray[imageIndex];

						if (image.HasMember("uri") && image["uri"].IsString()) {
							std::string uri = image["uri"].GetString();

							// Skip data URIs for now (embedded images)
							if (uri.find("data:") != 0) {
								// Resolve relative path
								std::string imagePath = resolveRelativePath(uri);
								textureToImagePath[static_cast<int>(i)] = imagePath;
							}
						}
					}
				}
			}

			// Now go through materials and load textures
			if (m_document.HasMember("materials") && m_document["materials"].IsArray()) {
				const rapidjson::Value& materialsArray = m_document["materials"];

				for (rapidjson::SizeType matIndex = 0; matIndex < materialsArray.Size() && matIndex < m_materials.size(); ++matIndex) {
					const rapidjson::Value& matJson = materialsArray[matIndex];
					auto& currentMaterial = m_materials[matIndex];

					// Parse PBR metallic roughness textures
					if (matJson.HasMember("pbrMetallicRoughness") && matJson["pbrMetallicRoughness"].IsObject()) {
						const rapidjson::Value& pbr = matJson["pbrMetallicRoughness"];

						// Base color texture
						if (pbr.HasMember("baseColorTexture") && pbr["baseColorTexture"].IsObject()) {
							const rapidjson::Value& baseColorTex = pbr["baseColorTexture"];
							if (baseColorTex.HasMember("index") && baseColorTex["index"].IsInt()) {
								int textureIndex = baseColorTex["index"].GetInt();
								auto pathIt = textureToImagePath.find(textureIndex);
								if (pathIt != textureToImagePath.end()) {
									currentMaterial.baseColorTexture =
										m_resourceManager->RequestResource<TextureResource>(pathIt->second);
								}
							}
						}

						// Metallic roughness texture
						if (pbr.HasMember("metallicRoughnessTexture") && pbr["metallicRoughnessTexture"].IsObject()) {
							const rapidjson::Value& metallicRoughnessTex = pbr["metallicRoughnessTexture"];
							if (metallicRoughnessTex.HasMember("index") && metallicRoughnessTex["index"].IsInt()) {
								int textureIndex = metallicRoughnessTex["index"].GetInt();
								auto pathIt = textureToImagePath.find(textureIndex);
								if (pathIt != textureToImagePath.end()) {
									currentMaterial.metallicRoughnessTexture =
										m_resourceManager->RequestResource<TextureResource>(pathIt->second);
								}
							}
						}
					}

					// Normal texture
					if (matJson.HasMember("normalTexture") && matJson["normalTexture"].IsObject()) {
						const rapidjson::Value& normalTex = matJson["normalTexture"];
						if (normalTex.HasMember("index") && normalTex["index"].IsInt()) {
							int textureIndex = normalTex["index"].GetInt();
							auto pathIt = textureToImagePath.find(textureIndex);
							if (pathIt != textureToImagePath.end()) {
								currentMaterial.normalTexture =
									m_resourceManager->RequestResource<TextureResource>(pathIt->second);
							}
						}
					}

					// Occlusion texture
					if (matJson.HasMember("occlusionTexture") && matJson["occlusionTexture"].IsObject()) {
						const rapidjson::Value& occlusionTex = matJson["occlusionTexture"];
						if (occlusionTex.HasMember("index") && occlusionTex["index"].IsInt()) {
							int textureIndex = occlusionTex["index"].GetInt();
							auto pathIt = textureToImagePath.find(textureIndex);
							if (pathIt != textureToImagePath.end()) {
								currentMaterial.occlusionTexture =
									m_resourceManager->RequestResource<TextureResource>(pathIt->second);
							}
						}
					}

					// Emissive texture
					if (matJson.HasMember("emissiveTexture") && matJson["emissiveTexture"].IsObject()) {
						const rapidjson::Value& emissiveTex = matJson["emissiveTexture"];
						if (emissiveTex.HasMember("index") && emissiveTex["index"].IsInt()) {
							int textureIndex = emissiveTex["index"].GetInt();
							auto pathIt = textureToImagePath.find(textureIndex);
							if (pathIt != textureToImagePath.end()) {
								currentMaterial.emissiveTexture =
									m_resourceManager->RequestResource<TextureResource>(pathIt->second);
							}
						}
					}
				}
			}

			return true;
		}
		catch (const std::exception& e) {
			std::cerr << "Exception while loading textures: " << e.what() << std::endl;
			return false;
		}
	}

	void GLTFResource::computeNodeTransforms() {
		if (m_scenes.empty() || m_defaultScene >= static_cast<int>(m_scenes.size())) {
			return;
		}

		const GLTFScene& scene = m_scenes[m_defaultScene];
		Matrix4f identity;
		identity.identity();

		for (int rootNodeIndex : scene.rootNodes) {
			computeNodeTransform(rootNodeIndex, identity);
		}
	}

	void GLTFResource::computeNodeTransform(int nodeIndex, const Matrix4f& parentTransform) {
		if (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_nodes.size())) {
			return;
		}

		GLTFNode& node = m_nodes[nodeIndex];
		node.worldTransform = parentTransform * node.matrix;

		for (int childIndex : node.children) {
			computeNodeTransform(childIndex, node.worldTransform);
		}
	}

	void GLTFResource::cleanup() {
		// Cleanup OpenGL resources
		for (auto& mesh : m_meshes) {
			mesh.cleanup();
		}
	}

	// Helper method to get buffer data
	template<typename T>
	const T* GLTFResource::getBufferData(int bufferViewIndex, int byteOffset) const {
		if (bufferViewIndex < 0 || bufferViewIndex >= static_cast<int>(m_bufferViews.size())) {
			return nullptr;
		}

		const GLTFBufferView& bufferView = m_bufferViews[bufferViewIndex];
		if (bufferView.bufferIndex < 0 || bufferView.bufferIndex >= static_cast<int>(m_buffers.size())) {
			return nullptr;
		}

		const auto& buffer = m_buffers[bufferView.bufferIndex];
		size_t offset = bufferView.byteOffset + byteOffset;

		if (offset >= buffer.size()) {
			return nullptr;
		}

		return reinterpret_cast<const T*>(buffer.data() + offset);
	}

	bool GLTFResource::parseBufferViews() {
		if (!m_document.HasMember("bufferViews")) {
			return true;
		}

		const rapidjson::Value& bufferViewsArray = m_document["bufferViews"];
		if (!bufferViewsArray.IsArray()) {
			std::cerr << "bufferViews is not an array" << std::endl;
			return false;
		}

		m_bufferViews.reserve(bufferViewsArray.Size());

		for (rapidjson::SizeType i = 0; i < bufferViewsArray.Size(); ++i) {
			const rapidjson::Value& bufferView = bufferViewsArray[i];
			GLTFBufferView view;

			if (bufferView.HasMember("buffer") && bufferView["buffer"].IsInt()) {
				view.bufferIndex = bufferView["buffer"].GetInt();
			}

			if (bufferView.HasMember("byteOffset") && bufferView["byteOffset"].IsUint64()) {
				view.byteOffset = bufferView["byteOffset"].GetUint64();
			}

			if (bufferView.HasMember("byteLength") && bufferView["byteLength"].IsUint64()) {
				view.byteLength = bufferView["byteLength"].GetUint64();
			}

			if (bufferView.HasMember("byteStride") && bufferView["byteStride"].IsUint64()) {
				view.byteStride = bufferView["byteStride"].GetUint64();
			}

			if (bufferView.HasMember("target") && bufferView["target"].IsUint()) {
				view.target = bufferView["target"].GetUint();
			}

			m_bufferViews.push_back(view);
		}

		return true;
	}

	bool GLTFResource::parseAccessors() {
		if (!m_document.HasMember("accessors")) {
			return true;
		}

		const rapidjson::Value& accessorsArray = m_document["accessors"];
		if (!accessorsArray.IsArray()) {
			std::cerr << "accessors is not an array" << std::endl;
			return false;
		}

		m_accessors.reserve(accessorsArray.Size());

		for (rapidjson::SizeType i = 0; i < accessorsArray.Size(); ++i) {
			const rapidjson::Value& accessor = accessorsArray[i];
			GLTFAccessor acc;

			if (accessor.HasMember("bufferView") && accessor["bufferView"].IsInt()) {
				acc.bufferViewIndex = accessor["bufferView"].GetInt();
			}

			if (accessor.HasMember("byteOffset") && accessor["byteOffset"].IsUint64()) {
				acc.byteOffset = accessor["byteOffset"].GetUint64();
			}

			if (accessor.HasMember("componentType") && accessor["componentType"].IsUint()) {
				acc.componentType = accessor["componentType"].GetUint();
			}

			if (accessor.HasMember("count") && accessor["count"].IsUint64()) {
				acc.count = accessor["count"].GetUint64();
			}

			if (accessor.HasMember("type") && accessor["type"].IsString()) {
				acc.type = accessor["type"].GetString();
			}

			m_accessors.push_back(acc);
		}

		return true;
	}

	bool GLTFResource::parseAccessorData(int accessorIndex, GLTFPrimitive& primitive) {
		if (accessorIndex < 0 || accessorIndex >= static_cast<int>(m_accessors.size())) {
			return false;
		}

		const GLTFAccessor& accessor = m_accessors[accessorIndex];
		primitive.indexData.indexCount = accessor.count;
		primitive.indexData.componentType = accessor.componentType;

		// Store index data without creating OpenGL buffers
		if (accessor.bufferViewIndex >= 0 && accessor.bufferViewIndex < static_cast<int>(m_bufferViews.size())) {
			const GLTFBufferView& bufferView = m_bufferViews[accessor.bufferViewIndex];

			if (bufferView.bufferIndex >= 0 && bufferView.bufferIndex < static_cast<int>(m_buffers.size())) {
				const auto& buffer = m_buffers[bufferView.bufferIndex];

				// Copy index data to primitive for later OpenGL buffer creation
				const uint8_t* data = buffer.data() + bufferView.byteOffset + accessor.byteOffset;
				primitive.indexData.indexData.resize(bufferView.byteLength);
				std::memcpy(primitive.indexData.indexData.data(), data, bufferView.byteLength);
			}
		}

		return true;
	}

	bool GLTFResource::parseAttributeData(const rapidjson::Value& attributes, GLTFPrimitive& primitive) {
		size_t vertexCount = 0;

		// Parse POSITION attribute first to determine vertex count
		if (attributes.HasMember("POSITION") && attributes["POSITION"].IsInt()) {
			int accessorIndex = attributes["POSITION"].GetInt();
			if (accessorIndex >= 0 && accessorIndex < static_cast<int>(m_accessors.size())) {
				const GLTFAccessor& accessor = m_accessors[accessorIndex];
				vertexCount = accessor.count;
				primitive.vertexData.vertexCount = vertexCount;

				if (accessor.type == "VEC3" && accessor.componentType == GL_FLOAT) {
					if (accessor.bufferViewIndex >= 0 && accessor.bufferViewIndex < static_cast<int>(m_bufferViews.size())) {
						const GLTFBufferView& bufferView = m_bufferViews[accessor.bufferViewIndex];
						if (bufferView.bufferIndex >= 0 && bufferView.bufferIndex < static_cast<int>(m_buffers.size())) {
							const auto& buffer = m_buffers[bufferView.bufferIndex];
							const float* positions = reinterpret_cast<const float*>(
								buffer.data() + bufferView.byteOffset + accessor.byteOffset);

							primitive.vertexData.positions.resize(vertexCount * 3);
							std::memcpy(primitive.vertexData.positions.data(), positions, vertexCount * 3 * sizeof(float));
						}
					}
				}
			}
		}

		// Parse NORMAL attribute
		if (attributes.HasMember("NORMAL") && attributes["NORMAL"].IsInt()) {
			int accessorIndex = attributes["NORMAL"].GetInt();
			if (accessorIndex >= 0 && accessorIndex < static_cast<int>(m_accessors.size())) {
				const GLTFAccessor& accessor = m_accessors[accessorIndex];

				if (accessor.type == "VEC3" && accessor.componentType == GL_FLOAT && accessor.count == vertexCount) {
					if (accessor.bufferViewIndex >= 0 && accessor.bufferViewIndex < static_cast<int>(m_bufferViews.size())) {
						const GLTFBufferView& bufferView = m_bufferViews[accessor.bufferViewIndex];
						if (bufferView.bufferIndex >= 0 && bufferView.bufferIndex < static_cast<int>(m_buffers.size())) {
							const auto& buffer = m_buffers[bufferView.bufferIndex];
							const float* normals = reinterpret_cast<const float*>(
								buffer.data() + bufferView.byteOffset + accessor.byteOffset);

							primitive.vertexData.normals.resize(vertexCount * 3);
							std::memcpy(primitive.vertexData.normals.data(), normals, vertexCount * 3 * sizeof(float));
						}
					}
				}
			}
		}

		// Parse all TEXCOORD_n attributes (support multiple texture coordinate sets)
		int texCoordIndex = 0;
		std::string texCoordName = "TEXCOORD_" + std::to_string(texCoordIndex);

		while (attributes.HasMember(texCoordName.c_str()) && attributes[texCoordName.c_str()].IsInt()) {
			int accessorIndex = attributes[texCoordName.c_str()].GetInt();
			if (accessorIndex >= 0 && accessorIndex < static_cast<int>(m_accessors.size())) {
				const GLTFAccessor& accessor = m_accessors[accessorIndex];

				if (accessor.type == "VEC2" && accessor.componentType == GL_FLOAT && accessor.count == vertexCount) {
					if (accessor.bufferViewIndex >= 0 && accessor.bufferViewIndex < static_cast<int>(m_bufferViews.size())) {
						const GLTFBufferView& bufferView = m_bufferViews[accessor.bufferViewIndex];
						if (bufferView.bufferIndex >= 0 && bufferView.bufferIndex < static_cast<int>(m_buffers.size())) {
							const auto& buffer = m_buffers[bufferView.bufferIndex];
							const float* texCoords = reinterpret_cast<const float*>(
								buffer.data() + bufferView.byteOffset + accessor.byteOffset);

							// Ensure we have enough texture coordinate sets
							if (primitive.vertexData.texCoords.size() <= static_cast<size_t>(texCoordIndex)) {
								primitive.vertexData.texCoords.resize(texCoordIndex + 1);
							}

							primitive.vertexData.texCoords[texCoordIndex].resize(vertexCount * 2);
							std::memcpy(primitive.vertexData.texCoords[texCoordIndex].data(), texCoords, vertexCount * 2 * sizeof(float));

							std::cout << "Loaded texture coordinate set " << texCoordIndex << " with " << vertexCount << " vertices" << std::endl;
						}
					}
				}
			}

			// Move to next texture coordinate set
			texCoordIndex++;
			texCoordName = "TEXCOORD_" + std::to_string(texCoordIndex);
		}

		// Set default values for missing attributes
		if (primitive.vertexData.normals.empty() && vertexCount > 0) {
			primitive.vertexData.normals.resize(vertexCount * 3);
			for (size_t i = 0; i < vertexCount; ++i) {
				primitive.vertexData.normals[i * 3 + 0] = 0.0f;
				primitive.vertexData.normals[i * 3 + 1] = 0.0f;
				primitive.vertexData.normals[i * 3 + 2] = 1.0f;
			}
		}

		// Ensure at least one texture coordinate set exists (even if empty)
		if (primitive.vertexData.texCoords.empty() && vertexCount > 0) {
			primitive.vertexData.texCoords.resize(1);
			primitive.vertexData.texCoords[0].resize(vertexCount * 2, 0.0f);
		}

		std::cout << "Parsed vertex data for primitive with " << vertexCount << " vertices and "
			<< primitive.vertexData.texCoords.size() << " texture coordinate sets" << std::endl;
		return vertexCount > 0;
	}

	bool GLTFResource::createOpenGLBuffers() {
		for (auto& mesh : m_meshes) {
			for (auto& primitive : mesh.primitives) {
				if (!createPrimitiveBuffers(primitive)) {
					std::cerr << "Failed to create buffers for primitive" << std::endl;
					return false;
				}
			}
		}
		return true;
	}

	bool GLTFResource::createPrimitiveBuffers(GLTFPrimitive& primitive) {
		if (primitive.vertexData.vertexCount == 0) {
			std::cerr << "No vertex data to create buffers from" << std::endl;
			return false;
		}

		// Create VAO
		glGenVertexArrays(1, &primitive.VAO);
		glBindVertexArray(primitive.VAO);

		// Create and populate vertex buffer
		glGenBuffers(1, &primitive.vertexBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, primitive.vertexBuffer);

		// Create interleaved vertex data structure that supports multiple texture coordinate sets
		struct VertexData {
			float position[3];
			float normal[3];
			float texCoord0[2]; // Primary texture coordinates
			float texCoord1[2]; // Secondary texture coordinates (if available)
			// Note: Could be extended further for more texture coordinate sets
		};

		std::vector<VertexData> vertices(primitive.vertexData.vertexCount);
		size_t numTexCoordSets = primitive.vertexData.texCoords.size();

		for (size_t i = 0; i < primitive.vertexData.vertexCount; ++i) {
			// Position
			if (i * 3 + 2 < primitive.vertexData.positions.size()) {
				vertices[i].position[0] = primitive.vertexData.positions[i * 3 + 0];
				vertices[i].position[1] = primitive.vertexData.positions[i * 3 + 1];
				vertices[i].position[2] = primitive.vertexData.positions[i * 3 + 2];
			}

			// Normal
			if (i * 3 + 2 < primitive.vertexData.normals.size()) {
				vertices[i].normal[0] = primitive.vertexData.normals[i * 3 + 0];
				vertices[i].normal[1] = primitive.vertexData.normals[i * 3 + 1];
				vertices[i].normal[2] = primitive.vertexData.normals[i * 3 + 2];
			}

			// Primary texture coordinates (TEXCOORD_0)
			if (numTexCoordSets > 0 && i * 2 + 1 < primitive.vertexData.texCoords[0].size()) {
				vertices[i].texCoord0[0] = primitive.vertexData.texCoords[0][i * 2 + 0];
				vertices[i].texCoord0[1] = primitive.vertexData.texCoords[0][i * 2 + 1];
			}
			else {
				vertices[i].texCoord0[0] = 0.0f;
				vertices[i].texCoord0[1] = 0.0f;
			}

			// Secondary texture coordinates (TEXCOORD_1)
			if (numTexCoordSets > 1 && i * 2 + 1 < primitive.vertexData.texCoords[1].size()) {
				vertices[i].texCoord1[0] = primitive.vertexData.texCoords[1][i * 2 + 0];
				vertices[i].texCoord1[1] = primitive.vertexData.texCoords[1][i * 2 + 1];
			}
			else {
				vertices[i].texCoord1[0] = 0.0f;
				vertices[i].texCoord1[1] = 0.0f;
			}
		}

		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(VertexData), vertices.data(), GL_STATIC_DRAW);

		// Set up vertex attribute pointers
		// Position (location 0)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, position));
		glEnableVertexAttribArray(0);

		// Normal (location 1)
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, normal));
		glEnableVertexAttribArray(1);

		// Primary texture coordinates (location 2)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, texCoord0));
		glEnableVertexAttribArray(2);

		// Secondary texture coordinates (location 3) - only enable if we have multiple sets
		if (numTexCoordSets > 1) {
			glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(VertexData), (void*)offsetof(VertexData, texCoord1));
			glEnableVertexAttribArray(3);
		}

		primitive.vertexCount = static_cast<GLsizei>(primitive.vertexData.vertexCount);

		// Create index buffer if we have index data
		if (!primitive.indexData.indexData.empty()) {
			glGenBuffers(1, &primitive.indexBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive.indexBuffer);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER,
				primitive.indexData.indexData.size(),
				primitive.indexData.indexData.data(),
				GL_STATIC_DRAW);

			primitive.indexCount = static_cast<GLsizei>(primitive.indexData.indexCount);
			primitive.indexComponentType = primitive.indexData.componentType;
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		std::cout << "Created OpenGL buffers for primitive with " << primitive.vertexCount << " vertices and "
			<< numTexCoordSets << " texture coordinate sets";
		if (primitive.indexCount > 0) {
			std::cout << " and " << primitive.indexCount << " indices";
		}
		std::cout << std::endl;

		// Keep the vertex data available for convex hull access - don't clear it
		// The convex hull was already generated during parsing

		return true;
	}

} // namespace ResourceSystem