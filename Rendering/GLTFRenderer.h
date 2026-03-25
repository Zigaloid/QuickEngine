#pragma once
#include "gl\glew.h"
#include "GLTFResource.h"
#include "math\Matrix4f.h"
#include <unordered_map>
#include <memory>

namespace RenderEngine {

	// GLTF Renderer - handles rendering of GLTF resources
	class GLTFRenderer {
	private:
		// Shader programs
		GLuint shaderProgram_ = 0;

		// Uniform locations
		struct UniformLocations {
			GLint mvpLocation = -1;
			GLint modelLocation = -1;
			GLint baseColorFactorLocation = -1;
			GLint hasBaseColorTextureLocation = -1;
			GLint baseColorTextureLocation = -1;
			GLint metallicFactorLocation = -1;
			GLint roughnessFactorLocation = -1;
		};

		UniformLocations uniformLocations_;

		// Shader creation helpers
		GLuint createShader(GLenum type, const char* source);
		GLuint createProgram(const char* vertexSource, const char* fragmentSource);
		bool createShaders();

		// Internal rendering methods
		void renderNode(
			const ResourceSystem::GLTFResource& resource,
			int nodeIndex, 
			const Matrix4f& viewProjectionMatrix, 
			const Matrix4f& parentTransform
		);

		void renderMesh(
			const ResourceSystem::GLTFMesh& mesh,
			const std::vector<ResourceSystem::GLTFMaterial>& materials,
			const Matrix4f& mvpMatrix, 
			const Matrix4f& modelMatrix
		);

		void renderPrimitive(
			const ResourceSystem::GLTFPrimitive& primitive, 
			const ResourceSystem::GLTFMaterial* material, 
			const Matrix4f& mvpMatrix, 
			const Matrix4f& modelMatrix
		);

		// Shader source code
		static const char* getVertexShaderSource();
		static const char* getFragmentShaderSource();

	public:
		GLTFRenderer();
		~GLTFRenderer();

		// Initialize the renderer (create shaders, etc.)
		bool Initialize();

		// Cleanup resources
		void Shutdown();

		// Main render method
		void Render(
			const ResourceSystem::GLTFResource& resource,
			const Matrix4f& viewProjectionMatrix,
			const Matrix4f& modelMatrix = Matrix4f()
		);

		// Render a specific scene
		void RenderScene(
			const ResourceSystem::GLTFResource& resource,
			int sceneIndex,
			const Matrix4f& viewProjectionMatrix,
			const Matrix4f& modelMatrix = Matrix4f()
		);

		// Check if renderer is ready
		bool IsReady() const { return shaderProgram_ != 0; }
	};

} // namespace RenderEngine