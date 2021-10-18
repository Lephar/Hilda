#include "Hilda.hpp"

GLFWwindow* window;
tinygltf::TinyGLTF objectLoader;
hld::Controls controls;
hld::Details details;
hld::State state;
hld::Camera camera;
glm::mat4 projection;
std::vector<GLushort> indices;
std::vector<hld::Vertex> vertices;
std::vector<std::string> imageNames;
std::vector<hld::Image> textures;
std::vector<hld::Mesh> meshes;
std::vector<hld::Portal> portals;
std::vector<glm::mat4> transforms;
GLuint VAO;
GLuint VBO;
GLuint EBO;
GLuint UBO;
GLuint shaderProgram;

void mouseCallback(GLFWwindow* handle, double x, double y) {
	static_cast<void>(handle);

	controls.deltaX += controls.mouseX - x;
	controls.deltaY += controls.mouseY - y;
	controls.mouseX = x;
	controls.mouseY = y;
}

void keyboardCallback(GLFWwindow* handle, int key, int scancode, int action, int mods) {
	static_cast<void>(scancode);
	static_cast<void>(mods);

	if (action == GLFW_RELEASE) {
		if (key == GLFW_KEY_W)
			controls.keyW = false;
		else if (key == GLFW_KEY_S)
			controls.keyS = false;
		else if (key == GLFW_KEY_A)
			controls.keyA = false;
		else if (key == GLFW_KEY_D)
			controls.keyD = false;
	}
	else if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_W)
			controls.keyW = true;
		else if (key == GLFW_KEY_S)
			controls.keyS = true;
		else if (key == GLFW_KEY_A)
			controls.keyA = true;
		else if (key == GLFW_KEY_D)
			controls.keyD = true;
		else if (key == GLFW_KEY_ESCAPE)
			glfwSetWindowShouldClose(handle, 1);
	}
}

void resizeEvent(GLFWwindow* handle, int width, int height) {
	static_cast<void>(width);
	static_cast<void>(height);

	glfwSetWindowSize(handle, details.width, details.width);
}

void initializeControls() {
	controls.keyW = false;
	controls.keyA = false;
	controls.keyS = false;
	controls.keyD = false;

	controls.deltaX = 0.0f;
	controls.deltaY = 0.0f;
}

uint32_t getTextureIndex(const tinygltf::Model& model, const tinygltf::Mesh& mesh) {
	auto& material = model.materials.at(mesh.primitives.front().material);

	for (auto& value : material.values)
		if (!value.first.compare("baseColorTexture"))
			return std::distance(imageNames.begin(), std::find(imageNames.begin(), imageNames.end(), model.images.at(value.second.TextureIndex()).name));

	return std::numeric_limits<uint32_t>::max();
}

glm::mat4 getNodeTranslation(const tinygltf::Node& node) {
	glm::mat4 translation{ 1.0f };

	for (auto index = 0u; index < node.translation.size(); index++)
		translation[3][index] = node.translation.at(index);

	return translation;
}

glm::mat4 getNodeRotation(const tinygltf::Node& node) {
	glm::mat4 rotation{ 1.0f };

	if (!node.rotation.empty())
		rotation = glm::toMat4(glm::qua{ node.rotation.at(3), node.rotation.at(0), node.rotation.at(1), node.rotation.at(2) });

	return rotation;
}

glm::mat4 getNodeScale(const tinygltf::Node& node) {
	glm::mat4 scale{ 1.0f };

	for (auto index = 0u; index < node.scale.size(); index++)
		scale[index][index] = node.scale.at(index);

	return scale;
}

glm::mat4 getNodeTransformation(const tinygltf::Node& node) {
	return getNodeTranslation(node) * getNodeRotation(node) * getNodeScale(node);
}

void createCameraFromMatrix(hld::Camera& camera, const glm::mat4& transformation, uint32_t room) {
	camera.room = room;
	camera.position = transformation * glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
	//camera.direction = transformation * glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f };
	camera.direction = glm::vec4{ 1.0f, 0.0f, 0.0f, 0.0f };
	camera.up = transformation * glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };
	camera.previous = camera.position;
}

void loadTexture(std::string name) {
	hld::Image image{};

	auto pixels = stbi_load(("assets/" + name + ".jpg").c_str(), &image.width, &image.height, &image.channel, STBI_rgb_alpha);
	image.channel = 4;

	glGenTextures(1, &image.texture);
	glBindTexture(GL_TEXTURE_2D, image.texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glGenerateMipmap(GL_TEXTURE_2D);

	textures.push_back(image);
	stbi_image_free(pixels);
}

void loadMesh(const tinygltf::Model& modelData, const tinygltf::Mesh& meshData, hld::Type type, uint32_t textureIndex,
	const glm::mat4& translation, const glm::mat4& rotation, const glm::mat4& scale, uint8_t room) {
	auto& primitive = meshData.primitives.front();
	auto& indexReference = modelData.bufferViews.at(primitive.indices);
	auto& indexData = modelData.buffers.at(indexReference.buffer);

	hld::Mesh mesh{};

	mesh.indexOffset = indices.size();
	mesh.indexLength = indexReference.byteLength / sizeof(GLushort);
	
	mesh.sourceRoom = room;
	mesh.sourceTransform = translation * rotation * scale;

	indices.resize(mesh.indexOffset + mesh.indexLength);
	std::memcpy(indices.data() + mesh.indexOffset, indexData.data.data() + indexReference.byteOffset, indexReference.byteLength);

	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<glm::vec2> texcoords;

	for (auto& attribute : primitive.attributes) {
		auto& accessor = modelData.accessors.at(attribute.second);
		auto& primitiveView = modelData.bufferViews.at(accessor.bufferView);
		auto& primitiveBuffer = modelData.buffers.at(primitiveView.buffer);

		if (attribute.first.compare("POSITION") == 0) {
			positions.resize(primitiveView.byteLength / sizeof(glm::vec3));
			std::memcpy(positions.data(), primitiveBuffer.data.data() + primitiveView.byteOffset, primitiveView.byteLength);
		}
		else if (attribute.first.compare("NORMAL") == 0) {
			normals.resize(primitiveView.byteLength / sizeof(glm::vec3));
			std::memcpy(normals.data(), primitiveBuffer.data.data() + primitiveView.byteOffset, primitiveView.byteLength);
		}
		else if (attribute.first.compare("TEXCOORD_0") == 0) {
			texcoords.resize(primitiveView.byteLength / sizeof(glm::vec2));
			std::memcpy(texcoords.data(), primitiveBuffer.data.data() + primitiveView.byteOffset, primitiveView.byteLength);
		}
	}

	mesh.vertexOffset = vertices.size();
	mesh.vertexLength = texcoords.size();
	mesh.textureIndex = textureIndex;

	for (auto index = 0u; index < mesh.vertexLength; index++) {
		hld::Vertex vertex{};
		vertex.position = mesh.sourceTransform * glm::vec4{ positions.at(index), 1.0f };
		vertex.normal = glm::normalize(glm::vec3{ mesh.sourceTransform * glm::vec4{ glm::normalize(normals.at(index)), 0.0f } });
		vertex.texture = texcoords.at(index);
		vertices.push_back(vertex);
	}

	auto min = glm::vec3{ std::numeric_limits<float_t>::max() }, max = glm::vec3{ -std::numeric_limits<float_t>::max() };

	for (auto index = 0u; index < mesh.vertexLength; index++) {
		auto& vertex = vertices.at(mesh.vertexOffset + index);

		min.x = std::min(min.x, vertex.position.x);
		min.y = std::min(min.y, vertex.position.y);
		min.z = std::min(min.z, vertex.position.z);

		max.x = std::max(max.x, vertex.position.x);
		max.y = std::max(max.y, vertex.position.y);
		max.z = std::max(max.z, vertex.position.z);
	}

	mesh.origin = glm::vec3{ mesh.sourceTransform * glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f } };
	mesh.minBorders = min;
	mesh.maxBorders = max;

	if (type == hld::Type::Mesh) {
		details.meshCount++;
		meshes.push_back(mesh);
	}

	else if (type == hld::Type::Portal) {
		hld::Portal portal{};

		portal.mesh = mesh;
		portal.direction = glm::normalize(glm::vec3(mesh.sourceTransform * glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f }));

		details.portalCount++;
		portals.push_back(portal);
	}
}

void loadModel(const std::string name, hld::Type type, uint8_t sourceRoom = 0, uint8_t targetRoom = 0) {
	std::string error, warning;
	tinygltf::Model model;

	auto result = objectLoader.LoadASCIIFromFile(&model, &error, &warning, "assets/" + name + ".gltf");

#ifndef NDEBUG
	if (!warning.empty())
		std::cout << "GLTF Warning: " << warning << std::endl;
	if (!error.empty())
		std::cout << "GLTF Error: " << error << std::endl;
	if (!result)
		return;
#endif

	if (type == hld::Type::Camera)
		createCameraFromMatrix(camera, getNodeTransformation(model.nodes.front()), sourceRoom);

	else {
		for (auto& image : model.images) {
			if (std::find(imageNames.begin(), imageNames.end(), image.name) == imageNames.end()) {
				imageNames.push_back(image.name);
				loadTexture(image.name);
			}
		}

		for (auto& node : model.nodes) {
			auto& mesh = model.meshes.at(node.mesh);
			loadMesh(model, mesh, type, getTextureIndex(model, mesh), getNodeTranslation(node), getNodeRotation(node), getNodeScale(node), sourceRoom);
		}

		if (type == hld::Type::Portal) {
			auto& bluePortal = portals.at(portals.size() - 2);
			auto& orangePortal = portals.at(portals.size() - 1);
			auto portalRotation = glm::rotate(glm::radians(180.0f), glm::vec3{ 0.0f, 0.0f, 1.0f });

			bluePortal.targetRoom = orangePortal.mesh.sourceRoom = targetRoom;
			orangePortal.targetRoom = bluePortal.mesh.sourceRoom = sourceRoom;

			bluePortal.targetTransform = orangePortal.mesh.sourceTransform;
			orangePortal.targetTransform = bluePortal.mesh.sourceTransform;

			bluePortal.cameraTransform = bluePortal.targetTransform * portalRotation * glm::inverse(bluePortal.mesh.sourceTransform);
			orangePortal.cameraTransform = orangePortal.targetTransform * portalRotation * glm::inverse(orangePortal.mesh.sourceTransform);
		}
	}
}

void createScene() {
	loadModel("camera", hld::Type::Camera, 1);

	loadModel("portal12", hld::Type::Portal, 1, 2);
	loadModel("portal13", hld::Type::Portal, 1, 3);
	loadModel("portal14", hld::Type::Portal, 1, 4);
	loadModel("portal15", hld::Type::Portal, 1, 5);
	loadModel("portal26", hld::Type::Portal, 2, 6);

	loadModel("room1", hld::Type::Mesh, 1);
	loadModel("room2", hld::Type::Mesh, 2);
	loadModel("room3", hld::Type::Mesh, 3);
	loadModel("room4", hld::Type::Mesh, 4);
	loadModel("room5", hld::Type::Mesh, 5);
	loadModel("room6", hld::Type::Mesh, 6);
}

GLuint createShader(std::string path, GLenum type)
{
	std::ifstream file;
	file.open(path.c_str());
	std::stringstream stream;
	stream << file.rdbuf();
	file.close();

	auto source = stream.str();
	auto code = source.c_str();

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &code, NULL);
	glCompileShader(shader);

#ifndef NDEBUG
	GLint result;
	GLchar log[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);

	if (!result)
	{
		glGetShaderInfoLog(shader, 512, NULL, log);
		std::cout << log << std::endl;
	}
#endif

	return shader;
}

GLuint createProgram(GLuint vertex, GLuint fragment)
{
	GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);

#ifndef NDEBUG
	int result;
	char log[512];
	glGetProgramiv(program, GL_LINK_STATUS, &result);

	if (!result)
	{
		glGetProgramInfoLog(program, 512, NULL, log);
		std::cout << log << std::endl;
	}
#endif

	return program;
}

void setup() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	details.width = 1280;
	details.height = 720;

    window = glfwCreateWindow(details.width, details.height, "", nullptr, nullptr);

    initializeControls();

	glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, keyboardCallback);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetFramebufferSizeCallback(window, resizeEvent);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &controls.mouseX, &controls.mouseY);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	createScene();

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);

	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glStencilMask(0xFF);

	glActiveTexture(GL_TEXTURE0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glClearStencil(0);
	glViewport(0, 0, details.width, details.height);

	GLuint vertexShader = createShader("shaders/vertex.vert", GL_VERTEX_SHADER);
	GLuint fragmentShader = createShader("shaders/fragment.frag", GL_FRAGMENT_SHADER);
	shaderProgram = createProgram(vertexShader, fragmentShader);

	glDetachShader(shaderProgram, vertexShader);
	glDetachShader(shaderProgram, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	glUseProgram(shaderProgram);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(hld::Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(hld::Vertex), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(hld::Vertex), (GLvoid*)sizeof(glm::vec3));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(hld::Vertex), (GLvoid*)(2 * sizeof(glm::vec3)));
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), indices.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &UBO);
	glBindBuffer(GL_UNIFORM_BUFFER, UBO);
	glUniformBlockBinding(shaderProgram, 0, 0);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);

	state.frameCount = 0;
	state.totalFrameCount = 0;
	state.currentImage = 0;
	state.checkPoint = 0.0f;
	state.recordingCount = 0;
	state.threadsActive = true;
	state.currentTime = std::chrono::high_resolution_clock::now();
	projection = glm::perspective(glm::radians(45.0f), static_cast<float_t>(details.width) / static_cast<float_t>(details.height),
		0.001f, 100.0f);
}

void drawMesh(hld::Mesh& mesh) {
	glBindTexture(GL_TEXTURE_2D, textures.at(mesh.textureIndex).texture);
	glDrawElementsBaseVertex(GL_TRIANGLES, mesh.indexLength, GL_UNSIGNED_SHORT, (GLvoid*)(mesh.indexOffset * sizeof(GLushort)), mesh.vertexOffset);
}

void gameTick() {
	state.previousTime = state.currentTime;
	state.currentTime = std::chrono::high_resolution_clock::now();
	state.timeDelta = std::chrono::duration<double_t, std::chrono::seconds::period>(state.currentTime - state.previousTime).count();
	state.checkPoint += state.timeDelta;

	auto moveDelta = state.timeDelta * 6.0, turnDelta = state.timeDelta * glm::radians(30.0);
	auto vectorCount = std::abs(controls.keyW - controls.keyS) + std::abs(controls.keyD - controls.keyA);

	if (vectorCount > 0)
		moveDelta /= std::sqrt(vectorCount);

	auto right = glm::normalize(glm::cross(camera.direction, camera.up));

	camera.direction = glm::normalize(glm::vec3{ glm::rotate<float_t>(turnDelta * controls.deltaY, right) *
														glm::rotate<float_t>(turnDelta * controls.deltaX, camera.up) *
														glm::vec4{camera.direction, 0.0f} });

	right = glm::normalize(glm::cross(camera.up, camera.direction));
	
	camera.previous = camera.position;
	camera.position += static_cast<float_t>(moveDelta * (controls.keyW - controls.keyS)) * camera.direction +
		static_cast<float_t>(moveDelta * (controls.keyA - controls.keyD)) * right;
	

	auto replacement = camera.position - camera.previous;
	auto direction = glm::normalize(replacement);
	auto coefficient = 0.0f, distance = glm::length(replacement);

	for (auto& portal : portals) {
		if (hld::epsilon < distance && glm::intersectRayPlane(camera.previous, direction, portal.mesh.origin, portal.direction, coefficient)) {
			auto point = camera.previous + coefficient * direction;

			if (point.x >= portal.mesh.minBorders.x && point.y >= portal.mesh.minBorders.y && point.z >= portal.mesh.minBorders.z &&
				point.x <= portal.mesh.maxBorders.x && point.y <= portal.mesh.maxBorders.y && point.z <= portal.mesh.maxBorders.z &&
				0 <= coefficient && distance >= coefficient) {
				camera.room = portal.targetRoom;

				camera.position = portal.cameraTransform * glm::vec4{ camera.position, 1.0f };
				camera.direction = portal.cameraTransform * glm::vec4{ camera.direction, 0.0f };
				camera.up = portal.cameraTransform * glm::vec4{ camera.up, 0.0f };

				camera.previous = camera.position;

				break;
			}
		}
	}

	controls.deltaX = 0.0f;
	controls.deltaY = 0.0f;

	auto transform = projection * glm::lookAt(camera.position, camera.position + camera.direction, camera.up);
	
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), &transform, GL_DYNAMIC_DRAW);
	//glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), &transform);

	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilFunc(GL_ALWAYS, 0, 0xFF);

	for (auto& mesh : meshes)
		drawMesh(mesh);

	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	
	for (int i = 0; i < details.portalCount; i++) {
		auto& portal = portals.at(i);

		glStencilFunc(GL_ALWAYS, i + 1, 0xFF);

		drawMesh(portal.mesh);
	}

	glClear(GL_DEPTH_BUFFER_BIT);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	
	for (int i = 0; i < details.portalCount; i++) {
		auto& portal = portals.at(i);

		auto portalCamera = camera;

		portalCamera.room = portal.targetRoom;
		portalCamera.position = portal.cameraTransform * glm::vec4{ portalCamera.position, 1.0f };
		portalCamera.direction = portal.cameraTransform * glm::vec4{ portalCamera.direction, 0.0f };

		auto portalTransform = projection * glm::lookAt(portalCamera.position, portalCamera.position + portalCamera.direction, portalCamera.up);

		glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), &portalTransform, GL_DYNAMIC_DRAW);
		glStencilFunc(GL_EQUAL, i + 1, 0xFF);
		
		for (auto& mesh : meshes)
			drawMesh(mesh);
	}

	state.frameCount++;
}

void draw() {
	while (true) {
		glfwPollEvents();

		if (glfwWindowShouldClose(window))
			break;

		gameTick();
		glfwSwapBuffers(window);

		if (state.checkPoint > 1.0) {
			state.totalFrameCount += state.frameCount;
			auto title = std::to_string(state.frameCount) + " - " + std::to_string(state.totalFrameCount);
			state.frameCount = 0;
			state.checkPoint = 0.0;
			glfwSetWindowTitle(window, title.c_str());
		}

		//std::this_thread::sleep_until(state.currentTime + std::chrono::milliseconds(1));
	}
}

void clean() {
	glfwTerminate();
}

int main()
{
	setup();
	draw();
	clean();
}
