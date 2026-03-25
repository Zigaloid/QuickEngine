#pragma once
#include "gl\glew.h"
#include "external/rapidjson/include/document.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include "TextureResource.h"
#include "math\Matrix4f.h"
#include "math\Vector3f.h"
#include "math\ConvexHull.h"  // Include the new ConvexHull header

namespace ResourceSystem {

	// Forward declarations
	struct GLTFMaterial;
	struct GLTFMesh;
	struct GLTFNode;
	struct GLTFScene;

	// Remove the old ConvexHull struct definition - we'll use Geometry::ConvexHull instead

	struct GLTFBufferView {
		int bufferIndex = 0;
		size_t byteOffset = 0;
		size_t byteLength = 0;
		size_t byteStride = 0;
		GLenum target = 0;
	};

	struct GLTFAccessor {
		int bufferViewIndex = -1;
		size_t byteOffset = 0;
		GLenum componentType = GL_FLOAT;
		size_t count = 0;
		std::string type; // "SCALAR", "VEC2", "VEC3", "VEC4", etc.
	};

	// GLTF Material structure
	struct GLTFMaterial {
		std::string name;

		// PBR Metallic Roughness
		Vector3f baseColorFactor = Vector3f(1.0f, 1.0f, 1.0f);
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;

		// Textures
		std::shared_ptr<TextureResource> baseColorTexture;
		std::shared_ptr<TextureResource> metallicRoughnessTexture;
		std::shared_ptr<TextureResource> normalTexture;
		std::shared_ptr<TextureResource> occlusionTexture;
		std::shared_ptr<TextureResource> emissiveTexture;

		// Emissive
		Vector3f emissiveFactor = Vector3f(0.0f, 0.0f, 0.0f);

		// Alpha
		enum class AlphaMode { OPAQUE, MASK, BLEND };
		AlphaMode alphaMode = AlphaMode::OPAQUE;
		float alphaCutoff = 0.5f;

		// Other properties
		bool doubleSided = false;
	};

	// Structures to store parsed vertex data before OpenGL buffer creation
	struct ParsedVertexData {
		std::vector<float> positions;
		std::vector<float> normals;
		std::vector<std::vector<float>> texCoords; // Support multiple texture coordinate sets
		size_t vertexCount = 0;
	};

	struct ParsedIndexData {
		std::vector<uint8_t> indexData;
		GLenum componentType = GL_UNSIGNED_SHORT;
		size_t indexCount = 0;
	};

	// GLTF Primitive (submesh) - contains OpenGL resources for rendering
	struct GLTFPrimitive {
		GLuint VAO = 0;
		GLuint vertexBuffer = 0;
		GLuint indexBuffer = 0;
		GLenum mode = GL_TRIANGLES;
		GLsizei indexCount = 0;
		GLsizei vertexCount = 0;
		GLenum indexComponentType = GL_UNSIGNED_SHORT;
		int materialIndex = -1;

		// Parsed data storage (kept for vertex access)
		ParsedVertexData vertexData;
		ParsedIndexData indexData;

		// Make movable but not copyable
		GLTFPrimitive() = default;
		GLTFPrimitive(const GLTFPrimitive&) = delete;
		GLTFPrimitive& operator=(const GLTFPrimitive&) = delete;
		GLTFPrimitive(GLTFPrimitive&&) = default;
		GLTFPrimitive& operator=(GLTFPrimitive&&) = default;

		void cleanup() {
			if (VAO) { glDeleteVertexArrays(1, &VAO); VAO = 0; }
			if (vertexBuffer) { glDeleteBuffers(1, &vertexBuffer); vertexBuffer = 0; }
			if (indexBuffer) { glDeleteBuffers(1, &indexBuffer); indexBuffer = 0; }
		}
	};

	// GLTF Mesh
	struct GLTFMesh {
		std::string name;
		std::vector<GLTFPrimitive> primitives;

		// Make movable but not copyable
		GLTFMesh() = default;
		GLTFMesh(const GLTFMesh&) = delete;
		GLTFMesh& operator=(const GLTFMesh&) = delete;
		GLTFMesh(GLTFMesh&&) = default;
		GLTFMesh& operator=(GLTFMesh&&) = default;

		void cleanup() {
			for (auto& primitive : primitives) {
				primitive.cleanup();
			}
		}

		// Extract all vertices from all primitives in this mesh
		std::vector<float> getVertices() const {
			std::vector<float> allVertices;
			
			for (const auto& primitive : primitives) {
				// Use the stored vertex data
				const auto& positions = primitive.vertexData.positions;
				allVertices.insert(allVertices.end(), positions.begin(), positions.end());
			}
			
			return allVertices;
		}
	};

	// GLTF Node
	struct GLTFNode {
		std::string name;

		// Transform
		Matrix4f matrix;
		Vector3f translation = Vector3f(0.0f, 0.0f, 0.0f);
		Vector3f scale = Vector3f(1.0f, 1.0f, 1.0f);
		// Note: quaternion rotation would go here in full implementation

		// Hierarchy
		std::vector<int> children;
		int parent = -1;

		// Content
		int meshIndex = -1;

		// Computed world transform
		Matrix4f worldTransform;
	};

	// GLTF Scene
	struct GLTFScene {
		std::string name;
		std::vector<int> rootNodes;
	};

	class GLTFResource : public Resource {
	private:
		// JSON data - using RapidJSON instead of nlohmann::json
		rapidjson::Document m_document;

		// Parsed GLTF data
		std::vector<GLTFMaterial> m_materials;
		std::vector<GLTFMesh> m_meshes;
		std::vector<GLTFNode> m_nodes;
		std::vector<GLTFScene> m_scenes;
		int m_defaultScene = 0;

		// Binary buffer data
		std::vector<std::vector<uint8_t>> m_buffers;

		// Reference to resource manager for loading textures
		ResourceManager* m_resourceManager = nullptr;

		// Resource-level convex hull (combines all meshes)
		std::unique_ptr<Geometry::ConvexHull> m_resourceConvexHull;

		std::vector<GLTFBufferView> m_bufferViews;
		std::vector<GLTFAccessor> m_accessors;

		// Helper methods for parsing
		bool parseBufferViews();
		bool parseAccessors();
		bool parseJSON();
		bool parseBuffers(FileSystem::FileSystemManager& fileSystem);
		bool parseMaterials();
		bool parseMeshes();
		bool parseNodes();
		bool parseScenes();
		bool loadTextures();
		void computeNodeTransforms();
		void computeNodeTransform(int nodeIndex, const Matrix4f& parentTransform);

		// Buffer and data parsing helpers
		bool parseDataURI(const std::string& uri, std::vector<uint8_t>& output);
		bool decodeBase64(const std::string& encoded, std::vector<uint8_t>& output);
		bool extractGLBBuffer(size_t bufferIndex, std::vector<uint8_t>& output);
		std::string resolveRelativePath(const std::string& relativePath);
		bool parseAccessorData(int accessorIndex, GLTFPrimitive& primitive);
		bool parseAttributeData(const rapidjson::Value& attributes, GLTFPrimitive& primitive);

		// OpenGL buffer creation (for rendering readiness)
		bool createOpenGLBuffers();
		bool createPrimitiveBuffers(GLTFPrimitive& primitive);

		// Convex hull generation methods - now using the new ConvexHullGenerator
		void generateConvexHull(); // Generate convex hulls resource-wide

		// Buffer access helpers
		template<typename T>
		const T* getBufferData(int bufferViewIndex, int byteOffset = 0) const;

		// Cleanup method
		void cleanup();

	public:
		GLTFResource(const std::string& path, ResourceManager* resourceManager = nullptr)
			: Resource(path), m_resourceManager(resourceManager) {
		}

		~GLTFResource() {
			cleanup();
		}

		// Resource lifecycle methods
		bool Initialize() override;
		bool Update(FileSystem::FileSystemManager& fileSystem) override;
		void Finalize() override;

		// Data access methods (for use by renderers)
		const std::vector<GLTFMaterial>& getMaterials() const { return m_materials; }
		const std::vector<GLTFMesh>& getMeshes() const { return m_meshes; }
		const std::vector<GLTFNode>& getNodes() const { return m_nodes; }
		const std::vector<GLTFScene>& getScenes() const { return m_scenes; }
		int getDefaultScene() const { return m_defaultScene; }

		// Get the resource-level convex hull (combines all meshes)
		const Geometry::ConvexHull* getResourceConvexHull() const { return m_resourceConvexHull.get(); }

		// Utility methods
		void setResourceManager(ResourceManager* resourceManager) { m_resourceManager = resourceManager; }

		// Check if resource is ready for rendering
		bool IsRenderReady() const { return IsFinalized(); }
	};

} // namespace ResourceSystem