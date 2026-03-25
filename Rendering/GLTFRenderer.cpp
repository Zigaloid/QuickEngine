#include "GLTFRenderer.h"
#include <iostream>

namespace RenderEngine {

	GLTFRenderer::GLTFRenderer() {
	}

	GLTFRenderer::~GLTFRenderer() {
		Shutdown();
	}

	bool GLTFRenderer::Initialize() {
		if (!createShaders()) {
			std::cerr << "GLTFRenderer: Failed to create shaders" << std::endl;
			return false;
		}

		std::cout << "GLTFRenderer: Initialized successfully" << std::endl;
		return true;
	}

	void GLTFRenderer::Shutdown() {
		if (shaderProgram_) {
			glDeleteProgram(shaderProgram_);
			shaderProgram_ = 0;
		}
	}

	const char* GLTFRenderer::getVertexShaderSource() {
		return R"(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord0;

uniform mat4 uMVP;
uniform mat4 uModel;

out vec3 worldPos;
out vec3 normal;
out vec2 texCoord;

void main() {
    vec4 worldPosition = uModel * vec4(aPosition, 1.0);
    worldPos = worldPosition.xyz;
    normal = mat3(uModel) * aNormal; // Note: should use normal matrix for non-uniform scaling
    texCoord = aTexCoord0;
    
    gl_Position = uMVP * vec4(aPosition, 1.0);
}
)";
	}

	const char* GLTFRenderer::getFragmentShaderSource() {
		return R"(
#version 330 core
in vec3 worldPos;
in vec3 normal;
in vec2 texCoord;

uniform vec3 uBaseColorFactor;
uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform bool uHasBaseColorTexture;
uniform sampler2D uBaseColorTexture;

out vec4 FragColor;

void main() {
    vec3 baseColor = uBaseColorFactor;
    
    if (uHasBaseColorTexture) {
        vec4 texColor = texture(uBaseColorTexture, texCoord);
        baseColor *= texColor.rgb;
    }
    
    // Simple lighting calculation
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 norm = normalize(normal);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor;
    
    vec3 ambient = 0.1 * baseColor;
    vec3 result = ambient + diffuse;
    
    FragColor = vec4(result, 1.0);
}
)";
	}

	bool GLTFRenderer::createShaders() {
		shaderProgram_ = createProgram(getVertexShaderSource(), getFragmentShaderSource());
		if (shaderProgram_ == 0) {
			return false;
		}

		// Get uniform locations
		uniformLocations_.mvpLocation = glGetUniformLocation(shaderProgram_, "uMVP");
		uniformLocations_.modelLocation = glGetUniformLocation(shaderProgram_, "uModel");
		uniformLocations_.baseColorFactorLocation = glGetUniformLocation(shaderProgram_, "uBaseColorFactor");
		uniformLocations_.hasBaseColorTextureLocation = glGetUniformLocation(shaderProgram_, "uHasBaseColorTexture");
		uniformLocations_.baseColorTextureLocation = glGetUniformLocation(shaderProgram_, "uBaseColorTexture");
		uniformLocations_.metallicFactorLocation = glGetUniformLocation(shaderProgram_, "uMetallicFactor");
		uniformLocations_.roughnessFactorLocation = glGetUniformLocation(shaderProgram_, "uRoughnessFactor");

		return true;
	}

	GLuint GLTFRenderer::createShader(GLenum type, const char* source) {
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		GLint success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			char infoLog[512];
			glGetShaderInfoLog(shader, 512, nullptr, infoLog);
			std::cerr << "GLTFRenderer: Shader compilation failed: " << infoLog << std::endl;
			glDeleteShader(shader);
			return 0;
		}

		return shader;
	}

	GLuint GLTFRenderer::createProgram(const char* vertexSource, const char* fragmentSource) {
		GLuint vertexShader = createShader(GL_VERTEX_SHADER, vertexSource);
		GLuint fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentSource);

		if (vertexShader == 0 || fragmentShader == 0) {
			if (vertexShader) glDeleteShader(vertexShader);
			if (fragmentShader) glDeleteShader(fragmentShader);
			return 0;
		}

		GLuint program = glCreateProgram();
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);

		GLint success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success) {
			char infoLog[512];
			glGetProgramInfoLog(program, 512, nullptr, infoLog);
			std::cerr << "GLTFRenderer: Program linking failed: " << infoLog << std::endl;
			glDeleteProgram(program);
			program = 0;
		}

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return program;
	}

	void GLTFRenderer::Render(
		const ResourceSystem::GLTFResource& resource,
		const Matrix4f& viewProjectionMatrix,
		const Matrix4f& modelMatrix
	) {
		if (!IsReady() || !resource.IsFinalized()) {
			return;
		}

		const auto& scenes = resource.getScenes();
		int defaultScene = resource.getDefaultScene();

		if (scenes.empty() || defaultScene >= static_cast<int>(scenes.size())) {
			return;
		}

		RenderScene(resource, defaultScene, viewProjectionMatrix, modelMatrix);
	}

	void GLTFRenderer::RenderScene(
		const ResourceSystem::GLTFResource& resource,
		int sceneIndex,
		const Matrix4f& viewProjectionMatrix,
		const Matrix4f& modelMatrix
	) {
		if (!IsReady() || !resource.IsFinalized()) {
			return;
		}

		const auto& scenes = resource.getScenes();
		if (sceneIndex < 0 || sceneIndex >= static_cast<int>(scenes.size())) {
			return;
		}

		glUseProgram(shaderProgram_);

		const ResourceSystem::GLTFScene& scene = scenes[sceneIndex];

		for (int rootNodeIndex : scene.rootNodes) {
			renderNode(resource, rootNodeIndex, viewProjectionMatrix, modelMatrix);
		}

		glUseProgram(0);
	}

	void GLTFRenderer::renderNode(
		const ResourceSystem::GLTFResource& resource,
		int nodeIndex,
		const Matrix4f& viewProjectionMatrix,
		const Matrix4f& parentTransform
	) {
		const auto& nodes = resource.getNodes();
		if (nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes.size())) {
			return;
		}

		const ResourceSystem::GLTFNode& node = nodes[nodeIndex];
		Matrix4f worldTransform = parentTransform * node.matrix;

		// Render mesh if present
		if (node.meshIndex >= 0 && node.meshIndex < static_cast<int>(resource.getMeshes().size())) {
			const ResourceSystem::GLTFMesh& mesh = resource.getMeshes()[node.meshIndex];
			Matrix4f mvpMatrix = viewProjectionMatrix * worldTransform;
			renderMesh(mesh, resource.getMaterials(), mvpMatrix, worldTransform);
		}

		// Render children
		for (int childIndex : node.children) {
			renderNode(resource, childIndex, viewProjectionMatrix, worldTransform);
		}
	}

	void GLTFRenderer::renderMesh(
		const ResourceSystem::GLTFMesh& mesh,
		const std::vector<ResourceSystem::GLTFMaterial>& materials,
		const Matrix4f& mvpMatrix,
		const Matrix4f& modelMatrix
	) {
		for (const ResourceSystem::GLTFPrimitive& primitive : mesh.primitives) {
			const ResourceSystem::GLTFMaterial* material = nullptr;
			if (primitive.materialIndex >= 0 && primitive.materialIndex < static_cast<int>(materials.size())) {
				material = &materials[primitive.materialIndex];
			}
			renderPrimitive(primitive, material, mvpMatrix, modelMatrix);
		}
	}

	void GLTFRenderer::renderPrimitive(
		const ResourceSystem::GLTFPrimitive& primitive,
		const ResourceSystem::GLTFMaterial* material,
		const Matrix4f& mvpMatrix,
		const Matrix4f& modelMatrix
	) {
		if (primitive.VAO == 0) {
			std::cerr << "GLTFRenderer: No VAO for primitive - cannot render" << std::endl;
			return;
		}

		// Set uniforms
		glUniformMatrix4fv(uniformLocations_.mvpLocation, 1, GL_FALSE, mvpMatrix.data());
		glUniformMatrix4fv(uniformLocations_.modelLocation, 1, GL_FALSE, modelMatrix.data());

		if (material) {
			glUniform3f(uniformLocations_.baseColorFactorLocation,
				material->baseColorFactor.x, material->baseColorFactor.y, material->baseColorFactor.z);
			glUniform1f(uniformLocations_.metallicFactorLocation, material->metallicFactor);
			glUniform1f(uniformLocations_.roughnessFactorLocation, material->roughnessFactor);

			// Bind base color texture if available
			if (material->baseColorTexture && material->baseColorTexture->IsFinalized()) {
				glUniform1i(uniformLocations_.hasBaseColorTextureLocation, 1);
				material->baseColorTexture->Bind(GL_TEXTURE0);
				glUniform1i(uniformLocations_.baseColorTextureLocation, 0);
			}
			else {
				glUniform1i(uniformLocations_.hasBaseColorTextureLocation, 0);
			}
		}
		else {
			// Default material
			glUniform3f(uniformLocations_.baseColorFactorLocation, 1.0f, 1.0f, 1.0f);
			glUniform1f(uniformLocations_.metallicFactorLocation, 0.0f);
			glUniform1f(uniformLocations_.roughnessFactorLocation, 1.0f);
			glUniform1i(uniformLocations_.hasBaseColorTextureLocation, 0);
		}

		// Render
		glBindVertexArray(primitive.VAO);

		if (primitive.indexCount > 0 && primitive.indexBuffer != 0) {
			// Indexed rendering
			glDrawElements(primitive.mode, primitive.indexCount, primitive.indexComponentType, 0);
		}
		else if (primitive.vertexCount > 0) {
			// Non-indexed rendering
			glDrawArrays(primitive.mode, 0, primitive.vertexCount);
		}

		glBindVertexArray(0);

		// Unbind texture
		if (material && material->baseColorTexture) {
			material->baseColorTexture->Unbind();
		}
	}

} // namespace RenderEngine