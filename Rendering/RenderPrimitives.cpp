
#include "RenderPrimitives.h"

// Shader source code
const char* BASIC_VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform vec3 uColor;

out vec3 vertexColor;
out vec3 normal;
out vec2 texCoord;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vertexColor = uColor;
    normal = aNormal;
    texCoord = aTexCoord;
}
)";

const char* BASIC_FRAGMENT_SHADER = R"(
#version 330 core
in vec3 vertexColor;
in vec3 normal;
in vec2 texCoord;

out vec4 FragColor;

void main() {

    vec3 baseColor = vertexColor;        
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



const char* TEXTURED_VERTEX_SHADER = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uMVP;
uniform vec3 uColor;

out vec3 vertexColor;
out vec3 normal;
out vec2 texCoord;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vertexColor = uColor;
    normal = aNormal;
    texCoord = aTexCoord;
}
)";

const char* TEXTURED_FRAGMENT_SHADER = R"(
#version 330 core
in vec3 vertexColor;
in vec3 normal;
in vec2 texCoord;

uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec4 texColor = texture(uTexture, texCoord);

    vec3 baseColor = vertexColor;
        
    // Simple lighting calculation
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 norm = normalize(normal);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor;
    
    vec3 ambient = 0.1 * baseColor;
    vec3 result = ambient + diffuse;
    
    
    FragColor = texColor * vec4(result, 1.0);
}
)";

bool Rendering::RenderPrimitives::CreateShaders()
{
	// Create basic shader program
	basicShaderProgram_ = CreateProgram(BASIC_VERTEX_SHADER, BASIC_FRAGMENT_SHADER);
	if (basicShaderProgram_ == 0) {
		return false;
	}

	// Get uniform locations for basic shader
	basicMVPLocation_ = glGetUniformLocation(basicShaderProgram_, "uMVP");
	basicColorLocation_ = glGetUniformLocation(basicShaderProgram_, "uColor");

	// Create textured shader program
	texturedShaderProgram_ = CreateProgram(TEXTURED_VERTEX_SHADER, TEXTURED_FRAGMENT_SHADER);
	if (texturedShaderProgram_ == 0) {
		return false;
	}

	// Get uniform locations for textured shader
	texturedMVPLocation_ = glGetUniformLocation(texturedShaderProgram_, "uMVP");
	texturedColorLocation_ = glGetUniformLocation(texturedShaderProgram_, "uColor");
	texturedSamplerLocation_ = glGetUniformLocation(texturedShaderProgram_, "uTexture");

	return true;
}

// Render cone
void Rendering::RenderPrimitives::RenderCone(const Vector3f& position, float radius, float height, const Vector3f& color, const Vector3f& rotation) {
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
	RenderGeometry(coneGeometry_);
}

void Rendering::RenderPrimitives::RenderCircleOutline(const Vector3f& center, float radius, const Vector3f& normal, const Vector3f& color, float width, int segments) {
	if (!isInitialized_ || segments < 3) return;

	// Create two perpendicular vectors in the plane perpendicular to the normal
	Vector3f tangent1, tangent2;
	
	// Find the most suitable tangent vectors based on the normal
	Vector3f normalizedNormal = normal.normalized();
	
	// Choose an initial vector that's not parallel to the normal
	Vector3f temp = (std::abs(normalizedNormal.getX()) < 0.9f) ? Vector3f(1.0f, 0.0f, 0.0f) : Vector3f(0.0f, 1.0f, 0.0f);
	
	// Create orthogonal basis using cross products
	tangent1 = normalizedNormal.cross(temp).normalized();
	tangent2 = normalizedNormal.cross(tangent1).normalized();

	// Generate points on the circle
	std::vector<Vector3f> circlePoints;
	circlePoints.reserve(segments);
	
	for (int i = 0; i < segments; ++i) {
		float angle = 2.0f * (float)M_PI * i / segments;
		
		// Calculate point on the circle using parametric circle equation
		Vector3f point = center + tangent1 * (radius * std::cos(angle)) + tangent2 * (radius * std::sin(angle));
		circlePoints.push_back(point);
	}

	// Draw lines between consecutive points to form the circle outline
	for (int i = 0; i < segments; ++i) {
		int nextIndex = (i + 1) % segments;
		RenderLine(circlePoints[i], circlePoints[nextIndex], color, width);
	}
}
