#include "RenderPrimitives.h"

// Generate quad geometry
void Rendering::RenderPrimitives::GenerateQuadGeometry() {
	// Quad vertices with position, normal, and UV coordinates
	float vertices[] = {
		// Position       // Normal        // UV
		-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
		 0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f
	};

	unsigned int indices[] = {
		0, 1, 2,
		2, 3, 0
	};

	glGenVertexArrays(1, &quadGeometry_.VAO);
	glGenBuffers(1, &quadGeometry_.VBO);
	glGenBuffers(1, &quadGeometry_.EBO);

	glBindVertexArray(quadGeometry_.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, quadGeometry_.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadGeometry_.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// UV attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	quadGeometry_.vertexCount = 4;
	quadGeometry_.indexCount = 6;
	quadGeometry_.hasIndices = true;
}

// Generate sphere geometry
void Rendering::RenderPrimitives::GenerateSphereGeometry() {
	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	// Generate sphere vertices
	for (int ring = 0; ring <= SPHERE_RINGS; ++ring) {
		float phi = (float)M_PI * ring / SPHERE_RINGS;
		float y = cos(phi);
		float ringRadius = sin(phi);

		for (int segment = 0; segment <= SPHERE_SEGMENTS; ++segment) {
			float theta = 2.0f * (float)M_PI * segment / SPHERE_SEGMENTS;
			float x = ringRadius * cos(theta);
			float z = ringRadius * sin(theta);

			// Position
			vertices.push_back(x);
			vertices.push_back(y);
			vertices.push_back(z);

			// Normal (same as position for unit sphere)
			vertices.push_back(x);
			vertices.push_back(y);
			vertices.push_back(z);

			// UV coordinates
			vertices.push_back((float)segment / SPHERE_SEGMENTS);
			vertices.push_back((float)ring / SPHERE_RINGS);
		}
	}

	// Generate indices
	for (int ring = 0; ring < SPHERE_RINGS; ++ring) {
		for (int segment = 0; segment < SPHERE_SEGMENTS; ++segment) {
			int current = ring * (SPHERE_SEGMENTS + 1) + segment;
			int next = current + SPHERE_SEGMENTS + 1;

			indices.push_back(current);
			indices.push_back(next);
			indices.push_back(current + 1);

			indices.push_back(current + 1);
			indices.push_back(next);
			indices.push_back(next + 1);
		}
	}

	glGenVertexArrays(1, &sphereGeometry_.VAO);
	glGenBuffers(1, &sphereGeometry_.VBO);
	glGenBuffers(1, &sphereGeometry_.EBO);

	glBindVertexArray(sphereGeometry_.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, sphereGeometry_.VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereGeometry_.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// UV attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	sphereGeometry_.vertexCount = (int)vertices.size() / 8;
	sphereGeometry_.indexCount = (int)indices.size();
	sphereGeometry_.hasIndices = true;
}

// Generate box geometry
void Rendering::RenderPrimitives::GenerateBoxGeometry() {
	float vertices[] = {
		// Front face
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,

		// Back face
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,

		// Left face
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,

		// Right face
		 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,

		 // Top face
		 -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
		  0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
		  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
		 -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f,

		 // Bottom face
		 -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
		  0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
		  0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
		 -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f
	};

	unsigned int indices[] = {
		// Front face
		0,  1,  2,   2,  3,  0,
		// Back face
		6,  5,  4,   4,  7,  6,
		// Left face
		8,  9,  10,  10, 11, 8,
		// Right face
		14, 13, 12,  12, 15, 14,
		// Top face
		18, 17, 16,  16, 19, 18,
		// Bottom face
		20, 21, 22,  22, 23, 20
	};

	glGenVertexArrays(1, &boxGeometry_.VAO);
	glGenBuffers(1, &boxGeometry_.VBO);
	glGenBuffers(1, &boxGeometry_.EBO);

	glBindVertexArray(boxGeometry_.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, boxGeometry_.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boxGeometry_.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// UV attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	boxGeometry_.vertexCount = 24;
	boxGeometry_.indexCount = 36;
	boxGeometry_.hasIndices = true;
}

// Generate circle geometry
void Rendering::RenderPrimitives::GenerateCircleGeometry() {
	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	// Center vertex
	vertices.insert(vertices.end(), { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f });

	// Generate circle vertices
	for (int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
		float angle = 2.0f * (float)M_PI * i / CIRCLE_SEGMENTS;
		float x = cos(angle);
		float z = sin(angle);

		// Position
		vertices.push_back(x);
		vertices.push_back(0.0f);
		vertices.push_back(z);

		// Normal
		vertices.push_back(0.0f);
		vertices.push_back(1.0f);
		vertices.push_back(0.0f);

		// UV
		vertices.push_back((x + 1.0f) * 0.5f);
		vertices.push_back((z + 1.0f) * 0.5f);
	}

	// Generate indices for triangles
	for (int i = 0; i < CIRCLE_SEGMENTS; ++i) {
		indices.push_back(0u);  // Center
		indices.push_back(static_cast<unsigned int>(i + 1));
		indices.push_back(static_cast<unsigned int>(i + 2));
	}

	glGenVertexArrays(1, &circleGeometry_.VAO);
	glGenBuffers(1, &circleGeometry_.VBO);
	glGenBuffers(1, &circleGeometry_.EBO);

	glBindVertexArray(circleGeometry_.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, circleGeometry_.VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, circleGeometry_.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// UV attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	circleGeometry_.vertexCount = (int)vertices.size() / 8;
	circleGeometry_.indexCount = (int)indices.size();
	circleGeometry_.hasIndices = true;
}

// Generate cylinder geometry
void Rendering::RenderPrimitives::GenerateCylinderGeometry() {
	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	float halfHeight = 0.5f;

	// Generate vertices for top and bottom circles
	for (int ring = 0; ring < 2; ++ring) {
		float y = ring == 0 ? -halfHeight : halfHeight;
		float normalY = ring == 0 ? -1.0f : 1.0f;

		// Center vertex
		vertices.insert(vertices.end(), { 0.0f, y, 0.0f, 0.0f, normalY, 0.0f, 0.5f, 0.5f });

		for (int i = 0; i <= CYLINDER_SEGMENTS; ++i) {
			float angle = 2.0f * (float)M_PI * i / CYLINDER_SEGMENTS;
			float x = cos(angle);
			float z = sin(angle);

			// Position
			vertices.push_back(x);
			vertices.push_back(y);
			vertices.push_back(z);

			// Normal (for caps)
			vertices.push_back(0.0f);
			vertices.push_back(normalY);
			vertices.push_back(0.0f);

			// UV
			vertices.push_back((x + 1.0f) * 0.5f);
			vertices.push_back((z + 1.0f) * 0.5f);
		}
	}

	// Side vertices
	for (int i = 0; i <= CYLINDER_SEGMENTS; ++i) {
		float angle = 2.0f * (float)M_PI * i / CYLINDER_SEGMENTS;
		float x = cos(angle);
		float z = sin(angle);

		// Bottom vertex
		vertices.insert(vertices.end(), { x, -halfHeight, z, x, 0.0f, z, (float)i / CYLINDER_SEGMENTS, 0.0f });

		// Top vertex
		vertices.insert(vertices.end(), { x, halfHeight, z, x, 0.0f, z, (float)i / CYLINDER_SEGMENTS, 1.0f });
	}

	// Generate indices
	int baseIndex = 0;

	// Bottom cap
	for (int i = 0; i < CYLINDER_SEGMENTS; ++i) {
		indices.insert(indices.end(), {
			static_cast<unsigned int>(baseIndex),
			static_cast<unsigned int>(baseIndex + i + 1),
			static_cast<unsigned int>(baseIndex + i + 2)
			});
	}
	baseIndex += CYLINDER_SEGMENTS + 2;

	// Top cap
	for (int i = 0; i < CYLINDER_SEGMENTS; ++i) {
		indices.insert(indices.end(), {
			static_cast<unsigned int>(baseIndex),
			static_cast<unsigned int>(baseIndex + i + 2),
			static_cast<unsigned int>(baseIndex + i + 1)
			});
	}
	baseIndex += CYLINDER_SEGMENTS + 2;

	// Sides
	for (int i = 0; i < CYLINDER_SEGMENTS; ++i) {
		unsigned int current = static_cast<unsigned int>(baseIndex + i * 2);
		unsigned int next = static_cast<unsigned int>(baseIndex + ((i + 1) % (CYLINDER_SEGMENTS + 1)) * 2);

		indices.insert(indices.end(), { current + 1u, next, current });
		indices.insert(indices.end(), { next + 1u, next, current + 1u });
	}

	glGenVertexArrays(1, &cylinderGeometry_.VAO);
	glGenBuffers(1, &cylinderGeometry_.VBO);
	glGenBuffers(1, &cylinderGeometry_.EBO);

	glBindVertexArray(cylinderGeometry_.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, cylinderGeometry_.VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cylinderGeometry_.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// UV attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	cylinderGeometry_.vertexCount = (int)vertices.size() / 8;
	cylinderGeometry_.indexCount = (int)indices.size();
	cylinderGeometry_.hasIndices = true;
}

// Generate cone geometry
void Rendering::RenderPrimitives::GenerateConeGeometry() {
	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	float halfHeight = 0.5f;

	// Apex vertex (top of cone)
	vertices.insert(vertices.end(), { 0.0f, halfHeight, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f });

	// Bottom center vertex
	vertices.insert(vertices.end(), { 0.0f, -halfHeight, 0.0f, 0.0f, -1.0f, 0.0f, 0.5f, 0.5f });

	// Generate bottom circle vertices
	for (int i = 0; i <= CONE_SEGMENTS; ++i) {
		float angle = 2.0f * (float)M_PI * i / CONE_SEGMENTS;
		float x = cos(angle);
		float z = sin(angle);

		// Bottom circle vertex
		vertices.push_back(x);
		vertices.push_back(-halfHeight);
		vertices.push_back(z);

		// Normal for bottom cap
		vertices.push_back(0.0f);
		vertices.push_back(-1.0f);
		vertices.push_back(0.0f);

		// UV for bottom cap
		vertices.push_back((x + 1.0f) * 0.5f);
		vertices.push_back((z + 1.0f) * 0.5f);
	}

	// Generate side vertices for the cone
	for (int i = 0; i <= CONE_SEGMENTS; ++i) {
		float angle = 2.0f * (float)M_PI * i / CONE_SEGMENTS;
		float x = cos(angle);
		float z = sin(angle);

		// Calculate normal for cone side (pointing outward and upward)
		Vector3f sideNormal = Vector3f(x, 0.5f, z).normalized();

		// Bottom vertex for side
		vertices.push_back(x);
		vertices.push_back(-halfHeight);
		vertices.push_back(z);
		vertices.push_back(sideNormal.x);
		vertices.push_back(sideNormal.y);
		vertices.push_back(sideNormal.z);
		vertices.push_back((float)i / CONE_SEGMENTS);
		vertices.push_back(0.0f);

		// Top vertex for side (apex)
		vertices.push_back(0.0f);
		vertices.push_back(halfHeight);
		vertices.push_back(0.0f);
		vertices.push_back(sideNormal.x);
		vertices.push_back(sideNormal.y);
		vertices.push_back(sideNormal.z);
		vertices.push_back((float)i / CONE_SEGMENTS);
		vertices.push_back(1.0f);
	}

	// Generate indices
	unsigned int baseIndex = 0;

	// Bottom cap indices - FIXED: correct winding order for bottom face (facing down)
	for (int i = 0; i < CONE_SEGMENTS; ++i) {
		indices.insert(indices.end(), {
			1u,  // Bottom center
			static_cast<unsigned int>(2 + i + 1),  // FIXED: swap order for correct winding
			static_cast<unsigned int>(2 + i)
			});
	}

	// Side indices - FIXED: correct winding order for outward-facing triangles
	baseIndex = 2 + CONE_SEGMENTS + 1; // Start of side vertices
	for (int i = 0; i < CONE_SEGMENTS; ++i) {
		unsigned int current = static_cast<unsigned int>(baseIndex + i * 2);
		unsigned int next = static_cast<unsigned int>(baseIndex + ((i + 1) % (CONE_SEGMENTS + 1)) * 2);

		indices.insert(indices.end(), {
			current,      // Bottom
			current + 1u, // Top (apex) - FIXED: swap order for counter-clockwise winding
			next          // Next bottom
			});
	}

	glGenVertexArrays(1, &coneGeometry_.VAO);
	glGenBuffers(1, &coneGeometry_.VBO);
	glGenBuffers(1, &coneGeometry_.EBO);

	glBindVertexArray(coneGeometry_.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, coneGeometry_.VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, coneGeometry_.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// UV attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	coneGeometry_.vertexCount = (int)vertices.size() / 8;
	coneGeometry_.indexCount = (int)indices.size();
	coneGeometry_.hasIndices = true;
}

// Generate line geometry
void Rendering::RenderPrimitives::GenerateLineGeometry() {
	float vertices[] = {
		// Position       // Normal        // UV
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f
	};

	glGenVertexArrays(1, &lineGeometry_.VAO);
	glGenBuffers(1, &lineGeometry_.VBO);

	glBindVertexArray(lineGeometry_.VAO);

	glBindBuffer(GL_ARRAY_BUFFER, lineGeometry_.VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	// Normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// UV attribute
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);

	lineGeometry_.vertexCount = 2;
	lineGeometry_.indexCount = 0;
	lineGeometry_.hasIndices = false;
}

void Rendering::RenderPrimitives::CleanupGeometry(GeometryData& geometry) {
	if (geometry.VAO != 0) {
		glDeleteVertexArrays(1, &geometry.VAO);
		geometry.VAO = 0;
	}
	if (geometry.VBO != 0) {
		glDeleteBuffers(1, &geometry.VBO);
		geometry.VBO = 0;
	}
	if (geometry.EBO != 0) {
		glDeleteBuffers(1, &geometry.EBO);
		geometry.EBO = 0;
	}
}
