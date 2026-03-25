#pragma once
#include "gl\glew.h"
#include <memory>
#include <vector>
#include <cmath>
#include <stack>
#include "TextureResource.h"
#include "math\Vector3f.h"
#include "math\Matrix4f.h"

namespace Rendering {

	class RenderPrimitives {
	private:
		// VAO and VBO storage for geometry
		struct GeometryData {
			GLuint VAO = 0;
			GLuint VBO = 0;
			GLuint EBO = 0;  // Element buffer for indexed rendering
			int vertexCount = 0;
			int indexCount = 0;
			bool hasIndices = false;
		};

		// Geometry data for each primitive type
		GeometryData quadGeometry_;
		GeometryData sphereGeometry_;
		GeometryData boxGeometry_;
		GeometryData circleGeometry_;
		GeometryData cylinderGeometry_;
		GeometryData lineGeometry_;
		GeometryData coneGeometry_;

		// Sphere subdivision parameters
		static constexpr int SPHERE_SEGMENTS = 32;
		static constexpr int SPHERE_RINGS = 16;

		// Circle segments
		static constexpr int CIRCLE_SEGMENTS = 32;

		// Cylinder parameters
		static constexpr int CYLINDER_SEGMENTS = 24;

		// Cone parameters
		static constexpr int CONE_SEGMENTS = 24;

		bool isInitialized_ = false;

		// Shader programs
		GLuint basicShaderProgram_ = 0;
		GLuint texturedShaderProgram_ = 0;

		// Uniform locations
		GLint basicMVPLocation_ = -1;
		GLint basicColorLocation_ = -1;
		GLint texturedMVPLocation_ = -1;
		GLint texturedColorLocation_ = -1;
		GLint texturedSamplerLocation_ = -1;

		// Matrix stack for transformations
		std::stack<Matrix4f> matrixStack_;
		Matrix4f currentMatrix_;

		// Current rendering state
		Matrix4f viewProjectionMatrix_;
		bool wireframeMode_ = false;

		// Helper methods to generate geometry
		void GenerateQuadGeometry();
		void GenerateSphereGeometry();
		void GenerateBoxGeometry();
		void GenerateCircleGeometry();
		void GenerateCylinderGeometry();
		void GenerateLineGeometry();
		void GenerateConeGeometry();

		// Cleanup geometry data
		void CleanupGeometry(GeometryData& geometry);

		// Shader creation and management
		bool CreateShaders();
		GLuint CreateShader(GLenum type, const char* source);
		GLuint CreateProgram(const char* vertexSource, const char* fragmentSource);
		void CleanupShaders();


		// Internal rendering helpers
		void RenderGeometry(const GeometryData& geometry, GLenum mode = GL_TRIANGLES);
		void SetupBasicShader(const Vector3f& color, const Matrix4f& modelMatrix);
		void SetupTexturedShader(const Vector3f& color, const Matrix4f& modelMatrix, ResourceSystem::TextureResource* texture);

	public:
		RenderPrimitives();
		~RenderPrimitives();

		// Initialize all geometry data and shaders
		bool Initialize();

		// Cleanup all resources
		void Shutdown();

		// Set the view-projection matrix (called by viewport)
		void SetViewProjectionMatrix(const Matrix4f& viewProjection);
		// Matrix stack operations
		void PushMatrix();
		void PopMatrix();
		void SetTransform(const Vector3f& position, const Vector3f& rotation = Vector3f::zero(), const Vector3f& scale = Vector3f::one());

		// Basic rendering primitives
		void RenderQuad(const Vector3f& position, const Vector3f& size, const Vector3f& color = Vector3f::one(), float rotation = 0.0f);

		void RenderSphere(const Vector3f& position, float radius, const Vector3f& color = Vector3f::one());

		void RenderBox(const Vector3f& position, const Vector3f& size, const Vector3f& color = Vector3f::one(), const Vector3f& rotation = Vector3f::zero());

		void RenderSprite(const Vector3f& position, const Vector3f& size, ResourceSystem::TextureResource* texture,
			const Vector3f& color = Vector3f::one(), float rotation = 0.0f, bool billboarded = false);

		void RenderCircle(const Vector3f& position, float radius, const Vector3f& color = Vector3f::one(), bool filled = true);

		void RenderCylinder(const Vector3f& position, float radius, float height, const Vector3f& color = Vector3f::one(), const Vector3f& rotation = Vector3f::zero());

		void RenderLine(const Vector3f& start, const Vector3f& end, const Vector3f& color = Vector3f::one(), float width = 1.0f);

		void RenderCircleOutline(const Vector3f& center, float radius, const Vector3f& normal, const Vector3f& color = Vector3f::one(), float width = 1.0f, int segments = 32);

		void RenderWireSphere(const Vector3f& position, float radius, const Vector3f& color = Vector3f::one());

		void RenderWireBox(const Vector3f& position, const Vector3f& size, const Vector3f& color = Vector3f::one(), const Vector3f& rotation = Vector3f::zero());

		void RenderCone(const Vector3f& position, float radius, float height, const Vector3f& color = Vector3f::one(), const Vector3f& rotation = Vector3f::zero());

		// Advanced rendering methods
		void RenderBillboard(const Vector3f& position, const Vector3f& size, ResourceSystem::TextureResource* texture,
			const Vector3f& color = Vector3f::one(), const Vector3f& cameraPosition = Vector3f::zero());

		void RenderTexturedQuad(const Vector3f& position, const Vector3f& size, ResourceSystem::TextureResource* texture,
			const Vector3f& color = Vector3f::one(), float rotation = 0.0f,
			const Vector3f& uvOffset = Vector3f::zero(), const Vector3f& uvScale = Vector3f::one());

		// Batch rendering for performance
		void BeginBatch();
		void EndBatch();

		// Utility methods
		void SetWireframeMode(bool enabled);
		void SetBlendMode(bool enabled, GLenum srcFactor = GL_SRC_ALPHA, GLenum dstFactor = GL_ONE_MINUS_SRC_ALPHA);
		void SetDepthTest(bool enabled);
		void SetCullFace(bool enabled, GLenum cullMode = GL_BACK);

		// Color and material helpers
		static Vector3f ColorFromRGB(int r, int g, int b) {
			return Vector3f(r / 255.0f, g / 255.0f, b / 255.0f);
		}

		static Vector3f ColorFromHex(unsigned int hex) {
			return Vector3f(
				((hex >> 16) & 0xFF) / 255.0f,
				((hex >> 8) & 0xFF) / 255.0f,
				(hex & 0xFF) / 255.0f
			);
		}

		// Common colors
		static Vector3f Red() { return Vector3f(1.0f, 0.0f, 0.0f); }
		static Vector3f Green() { return Vector3f(0.0f, 1.0f, 0.0f); }
		static Vector3f Blue() { return Vector3f(0.0f, 0.0f, 1.0f); }
		static Vector3f White() { return Vector3f(1.0f, 1.0f, 1.0f); }
		static Vector3f Black() { return Vector3f(0.0f, 0.0f, 0.0f); }
		static Vector3f Yellow() { return Vector3f(1.0f, 1.0f, 0.0f); }
		static Vector3f Magenta() { return Vector3f(1.0f, 0.0f, 1.0f); }
		static Vector3f Cyan() { return Vector3f(0.0f, 1.0f, 1.0f); }
	};

	
	// Implementation
	inline RenderPrimitives::RenderPrimitives() {
		// Initialize matrix stack with identity
		currentMatrix_.identity();
	}

	inline RenderPrimitives::~RenderPrimitives() {
		Shutdown();
	}

	inline bool RenderPrimitives::Initialize() {
		if (isInitialized_) {
			return true;
		}

		// Create shaders first
		if (!CreateShaders()) {
			return false;
		}

		// Generate all geometry
		GenerateQuadGeometry();
		GenerateSphereGeometry();
		GenerateBoxGeometry();
		GenerateCircleGeometry();
		GenerateCylinderGeometry();
		GenerateLineGeometry();
		GenerateConeGeometry();

		isInitialized_ = true;
		return true;
	}

	inline void RenderPrimitives::Shutdown() {
		if (!isInitialized_) {
			return;
		}

		CleanupGeometry(quadGeometry_);
		CleanupGeometry(sphereGeometry_);
		CleanupGeometry(boxGeometry_);
		CleanupGeometry(circleGeometry_);
		CleanupGeometry(cylinderGeometry_);
		CleanupGeometry(lineGeometry_);

		CleanupShaders();

		isInitialized_ = false;
	}

	inline GLuint RenderPrimitives::CreateShader(GLenum type, const char* source) {
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		// Check for compilation errors
		GLint success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			char infoLog[512];
			glGetShaderInfoLog(shader, 512, nullptr, infoLog);
			// In a real implementation, you'd use proper logging
			glDeleteShader(shader);
			return 0;
		}

		return shader;
	}

	inline GLuint RenderPrimitives::CreateProgram(const char* vertexSource, const char* fragmentSource) {
		GLuint vertexShader = CreateShader(GL_VERTEX_SHADER, vertexSource);
		GLuint fragmentShader = CreateShader(GL_FRAGMENT_SHADER, fragmentSource);

		if (vertexShader == 0 || fragmentShader == 0) {
			if (vertexShader) glDeleteShader(vertexShader);
			if (fragmentShader) glDeleteShader(fragmentShader);
			return 0;
		}

		GLuint program = glCreateProgram();
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);

		// Check for linking errors
		GLint success;
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success) {
			char infoLog[512];
			glGetProgramInfoLog(program, 512, nullptr, infoLog);
			// In a real implementation, you'd use proper logging
			glDeleteProgram(program);
			program = 0;
		}

		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		return program;
	}

	inline void RenderPrimitives::CleanupShaders() {
		if (basicShaderProgram_) {
			glDeleteProgram(basicShaderProgram_);
			basicShaderProgram_ = 0;
		}
		if (texturedShaderProgram_) {
			glDeleteProgram(texturedShaderProgram_);
			texturedShaderProgram_ = 0;
		}
	}

	inline void RenderPrimitives::SetViewProjectionMatrix(const Matrix4f& viewProjection) {
		viewProjectionMatrix_ = viewProjection;
	}

	inline void RenderPrimitives::PushMatrix() {
		matrixStack_.push(currentMatrix_);
	}

	inline void RenderPrimitives::PopMatrix() {
		if (!matrixStack_.empty()) {
			currentMatrix_ = matrixStack_.top();
			matrixStack_.pop();
		}
	}

	inline void RenderPrimitives::SetTransform(const Vector3f& position, const Vector3f& rotation, const Vector3f& scale) {
		// FIXED: Create transformation matrix in correct order: T * R * S
		Matrix4f transform;
		transform.identity();

		// Apply scale last (this will be applied first in the final transformation)
		transform = Matrix4f::scale(scale.x, scale.y, scale.z) * transform;

		// Apply rotations
		if (rotation.z != 0.0f) transform = Matrix4f::rotationZ(rotation.z * (float)M_PI / 180.0f) * transform;
		if (rotation.y != 0.0f) transform = Matrix4f::rotationY(rotation.y * (float)M_PI / 180.0f) * transform;
		if (rotation.x != 0.0f) transform = Matrix4f::rotationX(rotation.x * (float)M_PI / 180.0f) * transform;

		// Apply translation first (this will be applied last in the final transformation)
		transform = Matrix4f::translation(position.x, position.y, position.z) * transform;

		currentMatrix_ = transform;
	}

	inline void RenderPrimitives::SetupBasicShader(const Vector3f& color, const Matrix4f& modelMatrix) {
		glUseProgram(basicShaderProgram_);

		// FIXED: Apply currentMatrix_ (from matrix stack) to modelMatrix
		Matrix4f mvp = viewProjectionMatrix_ * currentMatrix_ * modelMatrix;
		glUniformMatrix4fv(basicMVPLocation_, 1, GL_FALSE, mvp.data());
		glUniform3f(basicColorLocation_, color.x, color.y, color.z);
	}

	inline void RenderPrimitives::SetupTexturedShader(const Vector3f& color, const Matrix4f& modelMatrix, ResourceSystem::TextureResource* texture) {
		glUseProgram(texturedShaderProgram_);

		// FIXED: Apply currentMatrix_ (from matrix stack) to modelMatrix
		Matrix4f mvp = viewProjectionMatrix_ * currentMatrix_ * modelMatrix;
		glUniformMatrix4fv(texturedMVPLocation_, 1, GL_FALSE, mvp.data());
		glUniform3f(texturedColorLocation_, color.x, color.y, color.z);

		if (texture && texture->GetTextureId() != 0) {
			glActiveTexture(GL_TEXTURE0);
			texture->Bind(GL_TEXTURE0);
			glUniform1i(texturedSamplerLocation_, 0);
		}
	}

	inline void RenderPrimitives::RenderGeometry(const GeometryData& geometry, GLenum mode) {
		if (geometry.VAO == 0) return;

		glBindVertexArray(geometry.VAO);

		if (geometry.hasIndices) {
			glDrawElements(mode, geometry.indexCount, GL_UNSIGNED_INT, 0);
		}
		else {
			glDrawArrays(mode, 0, geometry.vertexCount);
		}

		glBindVertexArray(0);
	}

	// Updated rendering methods using shaders
	inline void RenderPrimitives::RenderQuad(const Vector3f& position, const Vector3f& size, const Vector3f& color, float rotation) {
		if (!isInitialized_) return;

		// FIXED: Don't use matrix stack for individual primitive transforms
		Matrix4f modelMatrix;
		modelMatrix.identity();
		
		// Apply transformations in correct order: T * R * S
		modelMatrix = Matrix4f::translation(position.x, position.y, position.z) * modelMatrix;
		modelMatrix = Matrix4f::rotationZ(rotation * (float)M_PI / 180.0f) * modelMatrix;
		modelMatrix = Matrix4f::scale(size.x, size.y, 1.0f) * modelMatrix;

		SetupBasicShader(color, modelMatrix);
		RenderGeometry(quadGeometry_);
	}

	inline void RenderPrimitives::RenderSphere(const Vector3f& position, float radius, const Vector3f& color) {
		if (!isInitialized_) return;

		Matrix4f modelMatrix;
		modelMatrix.identity();
		
		modelMatrix = Matrix4f::scale(radius, radius, radius);
		modelMatrix = Matrix4f::translation(position.x, position.y, position.z) * modelMatrix;
		
		SetupBasicShader(color, modelMatrix);
		RenderGeometry(sphereGeometry_);
	}

	inline void RenderPrimitives::RenderBox(const Vector3f& position, const Vector3f& size, const Vector3f& color, const Vector3f& rotation) {
		if (!isInitialized_) return;

		Matrix4f modelMatrix;
		modelMatrix.identity();
		
		modelMatrix = Matrix4f::scale(size.x, size.y, size.z) * modelMatrix;
		modelMatrix = Matrix4f::translation(position.x, position.y, position.z) * modelMatrix;
		if (rotation.z != 0.0f) modelMatrix = Matrix4f::rotationZ(rotation.z * (float)M_PI / 180.0f) * modelMatrix;
		if (rotation.y != 0.0f) modelMatrix = Matrix4f::rotationY(rotation.y * (float)M_PI / 180.0f) * modelMatrix;
		if (rotation.x != 0.0f) modelMatrix = Matrix4f::rotationX(rotation.x * (float)M_PI / 180.0f) * modelMatrix;
		
		SetupBasicShader(color, modelMatrix);
		RenderGeometry(boxGeometry_);
	}

	inline void RenderPrimitives::RenderSprite(const Vector3f& position, const Vector3f& size, ResourceSystem::TextureResource* texture,
		const Vector3f& color, float rotation, bool billboarded) {
		if (!isInitialized_) return;

		if (billboarded) {
			RenderBillboard(position, size, texture, color);
		}
		else {
			Matrix4f modelMatrix;
			modelMatrix.identity();
			
			modelMatrix = Matrix4f::translation(position.x, position.y, position.z) * modelMatrix;
			modelMatrix = Matrix4f::rotationZ(rotation * (float)M_PI / 180.0f) * modelMatrix;
			modelMatrix = Matrix4f::scale(size.x, size.y, 1.0f) * modelMatrix;

			SetupTexturedShader(color, modelMatrix, texture);
			RenderGeometry(quadGeometry_);

			if (texture) texture->Unbind();
		}
	}

	inline void RenderPrimitives::RenderCircle(const Vector3f& position, float radius, const Vector3f& color, bool filled) {
		if (!isInitialized_) return;

		Matrix4f modelMatrix;
		modelMatrix.identity();
		
		modelMatrix = Matrix4f::translation(position.x, position.y, position.z) * modelMatrix;
		modelMatrix = Matrix4f::scale(radius, 1.0f, radius) * modelMatrix;

		SetupBasicShader(color, modelMatrix);

		if (filled) {
			RenderGeometry(circleGeometry_);
		}
		else {
			// Render wireframe
			bool wasWireframe = wireframeMode_;
			SetWireframeMode(true);
			RenderGeometry(circleGeometry_);
			SetWireframeMode(wasWireframe);
		}
	}

	inline void RenderPrimitives::RenderCylinder(const Vector3f& position, float radius, float height, const Vector3f& color, const Vector3f& rotation) {
		if (!isInitialized_) return;

		Matrix4f modelMatrix;
		modelMatrix.identity();

		// Apply transformations in correct order: T * R * S
		// Scale first (applied first in local space)
		modelMatrix = Matrix4f::scale(radius, height, radius) * modelMatrix;

		// Apply rotations in standard order: Z * Y * X (applied after scaling)
		if (rotation.x != 0.0f) modelMatrix = Matrix4f::rotationX(rotation.x * (float)M_PI / 180.0f) * modelMatrix;
		if (rotation.y != 0.0f) modelMatrix = Matrix4f::rotationY(rotation.y * (float)M_PI / 180.0f) * modelMatrix;
		if (rotation.z != 0.0f) modelMatrix = Matrix4f::rotationZ(rotation.z * (float)M_PI / 180.0f) * modelMatrix;

		// Translation last (applied last in world space)
		modelMatrix = Matrix4f::translation(position.x, position.y, position.z) * modelMatrix;

		SetupBasicShader(color, modelMatrix);
		RenderGeometry(cylinderGeometry_);
	}

	inline void RenderPrimitives::RenderLine(const Vector3f& start, const Vector3f& end, const Vector3f& color, float width) {
		if (!isInitialized_) return;

		Vector3f direction = end - start;
		float length = direction.magnitude();

		if (length < 0.001f) return;

		glLineWidth(width);
		direction = direction.normalized();

		Matrix4f modelMatrix;
		modelMatrix.identity();

		// Scale by length along X axis (since line geometry goes from (0,0,0) to (1,0,0))
		modelMatrix = Matrix4f::scale(length, 1.0f, 1.0f) * modelMatrix;

		// Create rotation matrix to align X-axis with the desired direction
		Vector3f xAxis = Vector3f(1, 0, 0);

		// Only apply rotation if direction is not already along X-axis
		if (std::abs(direction.dot(xAxis)) < 0.999f) {
			// Find a vector perpendicular to direction for the up vector
			Vector3f up;
			if (std::abs(direction.getY()) < 0.9f) {
				up = Vector3f(0, 1, 0);  // Use Y-up if direction isn't mostly vertical
			}
			else {
				up = Vector3f(0, 0, 1);  // Use Z-up if direction is mostly vertical
			}

			// Create orthonormal basis
			Vector3f right = direction.cross(up).normalized();
			up = right.cross(direction).normalized();

			// Create rotation matrix that transforms X-axis to direction
			Matrix4f rotation;
			rotation.identity();
			rotation.setColumn(0, direction);   // X-axis -> direction
			rotation.setColumn(1, up);          // Y-axis -> up
			rotation.setColumn(2, right);       // Z-axis -> right

			modelMatrix = rotation * modelMatrix;
		}

		// Apply translation to start position
		modelMatrix = Matrix4f::translation(start.x, start.y, start.z) * modelMatrix;

		SetupBasicShader(color, modelMatrix);
		RenderGeometry(lineGeometry_, GL_LINES);

		glLineWidth(1.0f);
	}
	
	inline void RenderPrimitives::RenderWireSphere(const Vector3f& position, float radius, const Vector3f& color) {
		if (!isInitialized_) return;

		bool wasWireframe = wireframeMode_;
		SetWireframeMode(true);

		RenderSphere(position, radius, color);

		SetWireframeMode(wasWireframe);
	}

	inline void RenderPrimitives::RenderWireBox(const Vector3f& position, const Vector3f& size, const Vector3f& color, const Vector3f& rotation) {
		if (!isInitialized_) return;

		bool wasWireframe = wireframeMode_;
		SetWireframeMode(true);

		RenderBox(position, size, color, rotation);

		SetWireframeMode(wasWireframe);
	}

	inline void RenderPrimitives::RenderBillboard(const Vector3f& position, const Vector3f& size, ResourceSystem::TextureResource* texture,
		const Vector3f& color, const Vector3f& cameraPosition) {
		if (!isInitialized_) return;

		// Calculate billboard transformation
		Vector3f forward = (cameraPosition - position).normalized();
		Vector3f up = Vector3f(0, 1, 0);  // Fixed: use proper up vector
		Vector3f right = up.cross(forward).normalized();
		up = forward.cross(right).normalized();

		// Create billboard matrix
		Matrix4f billboardMatrix;
		billboardMatrix.setColumn(0, Vector3f(right.x * size.x, right.y * size.x, right.z * size.x));
		billboardMatrix.setColumn(1, Vector3f(up.x * size.y, up.y * size.y, up.z * size.y));
		billboardMatrix.setColumn(2, Vector3f(forward.x, forward.y, forward.z));
		billboardMatrix.setColumn(3, Vector3f(position.x, position.y, position.z));

		SetupTexturedShader(color, billboardMatrix, texture);
		RenderGeometry(quadGeometry_);

		if (texture) texture->Unbind();
	}

	inline void RenderPrimitives::RenderTexturedQuad(const Vector3f& position, const Vector3f& size, ResourceSystem::TextureResource* texture,
		const Vector3f& color, float rotation, const Vector3f& uvOffset, const Vector3f& uvScale) {
		if (!isInitialized_) return;

		// Note: UV offset and scale would need additional shader support
		// For now, we'll render a basic textured quad
		Matrix4f modelMatrix;
		modelMatrix.identity();
		
		modelMatrix = Matrix4f::translation(position.x, position.y, position.z) * modelMatrix;
		modelMatrix = Matrix4f::rotationZ(rotation * (float)M_PI / 180.0f) * modelMatrix;
		modelMatrix = Matrix4f::scale(size.x, size.y, 1.0f) * modelMatrix;

		SetupTexturedShader(color, modelMatrix, texture);
		RenderGeometry(quadGeometry_);

		if (texture) texture->Unbind();
	}

	inline void RenderPrimitives::BeginBatch() {
		// For batching, you would typically accumulate geometry data
		// and submit it all at once. This is a placeholder implementation.
	}

	inline void RenderPrimitives::EndBatch() {
		// Submit all batched geometry
		// This would flush any accumulated draw calls
	}

	inline void RenderPrimitives::SetWireframeMode(bool enabled) {
		wireframeMode_ = enabled;
		if (enabled) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		else {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		}
	}

	inline void RenderPrimitives::SetBlendMode(bool enabled, GLenum srcFactor, GLenum dstFactor) {
		if (enabled) {
			glEnable(GL_BLEND);
			glBlendFunc(srcFactor, dstFactor);
		}
		else {
			glDisable(GL_BLEND);
		}
	}

	inline void RenderPrimitives::SetDepthTest(bool enabled) {
		if (enabled) {
			glEnable(GL_DEPTH_TEST);
		}
		else {
			glDisable(GL_DEPTH_TEST);
		}
	}

	inline void RenderPrimitives::SetCullFace(bool enabled, GLenum cullMode) {
		if (enabled) {
			glEnable(GL_CULL_FACE);
			glCullFace(cullMode);
		}
		else {
			glDisable(GL_CULL_FACE);
		}
	}

} // namespace Rendering