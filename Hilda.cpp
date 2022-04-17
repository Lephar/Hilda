#include "Hilda.hpp"

GLFWwindow* window;

tinygltf::TinyGLTF objectLoader;

std::string assetFolder;
std::string shaderFolder;

bool VR;

ovrSession session;
ovrGraphicsLuid luid;
ovrHmdDesc hmdDesc;
ovrTextureSwapChain swapchain;
ovrEyeRenderDesc eyeRenderDesc[2];
ovrPosef hmdToEyeViewPose[2];
ovrLayerEyeFov layer;
std::unordered_map<int, GLuint> framebuffers;

Controls controls;
Details details;
State state;
Camera camera;

glm::vec3 origin;
glm::vec4 forward;
glm::mat4 projection;

std::vector<GLushort> indices;
std::vector<Vertex> vertices;
std::vector<std::string> imageNames;
std::vector<Image> textures;
std::vector<Mesh> meshes;
std::vector<Portal> portals;
std::vector<Node> nodes;

GLuint FBO;
GLuint VAO;
GLuint VBO;
GLuint EBO;
GLuint UBO;
GLuint shaderProgram;

std::ostream& operator<<(std::ostream& os, glm::vec2& vector) {
	return os << vector.x << " " << vector.y << std::endl;
}

std::ostream& operator<<(std::ostream& os, glm::vec3& vector) {
	return os << vector.x << " " << vector.y << " " << vector.z << std::endl;
}

std::ostream& operator<<(std::ostream& os, Vertex& vertex) {
	return os << vertex.position << vertex.normal << vertex.texture << std::endl;
}

std::ostream& operator<<(std::ostream& os, Node& node) {
	return os << node.layer << " " << node.parentIndex << " " << node.portalIndex << std::endl;
}

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

	glfwSetWindowSize(handle, details.windowWidth, details.windowHeight);
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

void createCameraFromMatrix(Camera& camera, const glm::mat4& transformation, uint32_t room) {
	camera.room = room;
	camera.position = transformation * glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
	camera.direction = transformation * glm::vec4{ 0.0f, -1.0f, 0.0f, 0.0f };
	camera.up = transformation * glm::vec4{ 0.0f, 0.0f, 1.0f, 0.0f };
	camera.previous = camera.position;
}

void loadTexture(std::string name) {
	Image image{};

	auto pixels = stbi_load((assetFolder + name + ".jpg").c_str(), &image.width, &image.height, &image.channel, STBI_rgb_alpha);
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

void loadMesh(const tinygltf::Model& modelData, const tinygltf::Mesh& meshData, Type type,
	const glm::mat4& translation, const glm::mat4& rotation, const glm::mat4& scale, uint8_t room) {
	for(auto& primitive : meshData.primitives) {
		auto& indexReference = modelData.bufferViews.at(primitive.indices);
		auto& indexData = modelData.buffers.at(indexReference.buffer);
		auto& material = modelData.materials.at(primitive.material);
		auto textureIndex = 0u;

		for (auto& value : material.values) {
			if (!value.first.compare("baseColorTexture")) {
				textureIndex = std::distance(imageNames.begin(), std::find(imageNames.begin(), imageNames.end(),
					modelData.images.at(value.second.TextureIndex()).name));
			}
		}

		Mesh mesh{};

		mesh.indexOffset = indices.size();
		mesh.indexLength = indexReference.byteLength / sizeof(GLushort);
	
		mesh.room = room;
		mesh.transform = translation * rotation * scale;

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
			Vertex vertex{};
			vertex.position = mesh.transform * glm::vec4{ positions.at(index), 1.0f };
			vertex.normal = glm::normalize(glm::vec3{ mesh.transform * glm::vec4{ glm::normalize(normals.at(index)), 0.0f } });
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

		mesh.origin = glm::vec3{ mesh.transform * glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f } };
		mesh.minBorders = min;
		mesh.maxBorders = max;

		if (type == Type::Mesh) {
			details.meshCount++;
			meshes.push_back(mesh);
		}

		else if (type == Type::Portal) {
			Portal portal{};

			portal.mesh = mesh;
			portal.direction = glm::normalize(glm::vec3(mesh.transform * glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f }));

			details.portalCount++;
			portals.push_back(portal);
		}
	}
}

void loadModel(const std::string name, Type type, uint8_t sourceRoom = 0, uint8_t targetRoom = 0) {
	std::string error, warning;
	tinygltf::Model model;

	auto result = objectLoader.LoadASCIIFromFile(&model, &error, &warning, assetFolder + name + ".gltf");

#ifndef NDEBUG
	if (!warning.empty())
		std::cout << "GLTF Warning: " << warning << std::endl;
	if (!error.empty())
		std::cout << "GLTF Error: " << error << std::endl;
	if (!result)
		return;
#endif

	if (type == Type::Camera) {
		createCameraFromMatrix(camera, getNodeTransformation(model.nodes.front()), sourceRoom);

		origin = camera.position;
		forward = glm::vec4{ camera.direction, 0.0f };

		projection = glm::perspective(glm::radians(45.0f),
			static_cast<float_t>(details.windowWidth) / static_cast<float_t>(details.windowHeight), 0.01f, 1000.0f);
	}

	else {
		for (auto& image : model.images) {
			if (std::find(imageNames.begin(), imageNames.end(), image.name) == imageNames.end()) {
				imageNames.push_back(image.name);
				loadTexture(image.name);
			}
		}

		for (auto& node : model.nodes) {
			auto& mesh = model.meshes.at(node.mesh);
			loadMesh(model, mesh, type, getNodeTranslation(node), getNodeRotation(node), getNodeScale(node), sourceRoom);
		}

		if (type == Type::Portal) {
			auto& bluePortal = portals.at(portals.size() - 2);
			auto& orangePortal = portals.at(portals.size() - 1);
			auto portalRotation = glm::rotate(glm::radians(180.0f), glm::vec3{ 0.0f, 0.0f, 1.0f });

			bluePortal.targetRoom = orangePortal.mesh.room = targetRoom;
			orangePortal.targetRoom = bluePortal.mesh.room = sourceRoom;

			bluePortal.cameraTransform = orangePortal.mesh.transform * portalRotation * glm::inverse(bluePortal.mesh.transform);
			orangePortal.cameraTransform = bluePortal.mesh.transform * portalRotation * glm::inverse(orangePortal.mesh.transform);
		}
	}
}

void createScene() {
	/*
	assetFolder = "Assets/backroom/";
	
	loadModel("camera", Type::Camera, 1);

	loadModel("portal12", Type::Portal, 1, 2);
	loadModel("portal13", Type::Portal, 1, 3);
	loadModel("portal14", Type::Portal, 1, 4);
	loadModel("portal15", Type::Portal, 1, 5);
	loadModel("portal26", Type::Portal, 2, 6);

	loadModel("room1", Type::Mesh, 1);
	loadModel("room2", Type::Mesh, 2);
	loadModel("room3", Type::Mesh, 3);
	loadModel("room4", Type::Mesh, 4);
	loadModel("room5", Type::Mesh, 5);
	loadModel("room6", Type::Mesh, 6);
	*/
	
	// TODO: Fix portal association
	assetFolder = "Assets/italy/";

	loadModel("camera", Type::Camera, 2);

	loadModel("portal12", Type::Portal, 2, 1);
	loadModel("portal23", Type::Portal, 3, 2);
	loadModel("portal34", Type::Portal, 4, 3);
	loadModel("portal45", Type::Portal, 5, 4);
	loadModel("portal56", Type::Portal, 6, 5);
	loadModel("portal67", Type::Portal, 7, 6);
	loadModel("portal78", Type::Portal, 8, 7);
	loadModel("portal89", Type::Portal, 9, 8);
	loadModel("portal9A", Type::Portal, 10, 9);
	loadModel("portalA1", Type::Portal, 1, 10);

	loadModel("room1", Type::Mesh, 1);
	loadModel("room2", Type::Mesh, 2);
	loadModel("room3", Type::Mesh, 3);
	loadModel("room4", Type::Mesh, 4);
	loadModel("room5", Type::Mesh, 5);
	loadModel("room6", Type::Mesh, 6);
	loadModel("room7", Type::Mesh, 7);
	loadModel("room8", Type::Mesh, 8);
	loadModel("room9", Type::Mesh, 9);
	loadModel("roomA", Type::Mesh, 10);
}

GLuint createShader(std::string path, GLenum type)
{
	std::ifstream file;
	file.open((shaderFolder + path).c_str());
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

void initializeStates() {
	details.windowWidth = 800;
	details.windowHeight = 600;
	details.maxCameraCount = 15;	// 2 ^ 4 - 1 - 1

	controls.keyW = false;
	controls.keyA = false;
	controls.keyS = false;
	controls.keyD = false;
	controls.deltaX = 0.0f;
	controls.deltaY = 0.0f;

	state.currentImage = 0;
	state.frameCount = 0;
	state.totalFrameCount = 0;
	state.checkPoint = 0.0f;
	state.currentTime = std::chrono::high_resolution_clock::now();
}

void initializeContext() {
	ovr_Initialize(nullptr);
	VR = OVR_SUCCESS(ovr_Create(&session, &luid));

	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);

	window = glfwCreateWindow(details.windowWidth, details.windowHeight, "", nullptr, nullptr);

	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, keyboardCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetFramebufferSizeCallback(window, resizeEvent);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwGetCursorPos(window, &controls.mouseX, &controls.mouseY);
	glfwSwapInterval(0);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
}

GLuint createFramebufferRenderBuffer(uint32_t width, uint32_t height) {
	GLuint renderBuffer;

	glGenRenderbuffers(1, &renderBuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH32F_STENCIL8, width, height);

	return renderBuffer;
}

GLuint createFramebufferColorTexture(uint32_t width, uint32_t height) {
	GLuint colorTexture;

	glGenTextures(1, &colorTexture);
	glBindTexture(GL_TEXTURE_2D, colorTexture);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return colorTexture;
}

GLuint createFramebuffer(GLuint renderBuffer, GLuint colorTexture) {
	GLuint framebuffer;

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	glBindRenderbuffer(GL_RENDERBUFFER, renderBuffer);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderBuffer);

	glBindTexture(GL_TEXTURE_2D, colorTexture);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

	return framebuffer;
}

void setupGraphics() {
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glClearDepth(1.0f);

	glEnable(GL_STENCIL_TEST);
	glClearStencil(0);

	glClearColor(0.4f, 0.8f, 1.0f, 1.0f);

	shaderFolder = "Shaders/";
	GLuint vertexShader = createShader("vertex.vert", GL_VERTEX_SHADER);
	GLuint fragmentShader = createShader("fragment.frag", GL_FRAGMENT_SHADER);
	shaderProgram = createProgram(vertexShader, fragmentShader);

	glDetachShader(shaderProgram, vertexShader);
	glDetachShader(shaderProgram, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	glUseProgram(shaderProgram);

	if(VR) {
		hmdDesc = ovr_GetHmdDesc(session);

		ovrSizei leftEyeSize = ovr_GetFovTextureSize(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0], 1.0f);
		ovrSizei rightEyeSize = ovr_GetFovTextureSize(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1], 1.0f);

		details.headsetWidth = leftEyeSize.w + rightEyeSize.w;
		details.headsetHeight = std::max(leftEyeSize.h, rightEyeSize.h);

		ovrTextureSwapChainDesc swapchainDesc{};
		swapchainDesc.Type = ovrTexture_2D;
		swapchainDesc.ArraySize = 1;
		swapchainDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		swapchainDesc.Width = details.headsetWidth;
		swapchainDesc.Height = details.headsetHeight;
		swapchainDesc.MipLevels = 1;
		swapchainDesc.SampleCount = 1;
		swapchainDesc.StaticImage = ovrFalse;

		ovr_CreateTextureSwapChainGL(session, &swapchainDesc, &swapchain);
		ovr_GetTextureSwapChainLength(session, swapchain, &details.headsetImageCount);

		for (int imageIndex = 0; imageIndex < details.headsetImageCount; imageIndex++) {
			GLuint swapchainColorTexture;
			ovr_GetTextureSwapChainBufferGL(session, swapchain, imageIndex, &swapchainColorTexture);

			GLuint swapchainRenderBuffer = createFramebufferRenderBuffer(details.headsetWidth, details.headsetHeight);
			GLuint swapchainFramebuffer = createFramebuffer(swapchainRenderBuffer, swapchainColorTexture);

			framebuffers.emplace(imageIndex, swapchainFramebuffer);
		}
		
		eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
		eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);
		hmdToEyeViewPose[0] = eyeRenderDesc[0].HmdToEyePose;
		hmdToEyeViewPose[1] = eyeRenderDesc[1].HmdToEyePose;

		layer.Header.Type = ovrLayerType_EyeFov;
		layer.Header.Flags = 0;
		layer.ColorTexture[0] = swapchain;
		layer.ColorTexture[1] = swapchain;
		layer.Fov[0] = eyeRenderDesc[0].Fov;
		layer.Fov[1] = eyeRenderDesc[1].Fov;
		layer.Viewport[0] = ovrRecti(ovrVector2i(0, 0), ovrSizei(details.headsetWidth / 2, details.headsetHeight));
		layer.Viewport[1] = ovrRecti(ovrVector2i(details.headsetWidth / 2, 0), ovrSizei(details.headsetWidth / 2, details.headsetHeight));
	}

	GLuint renderBuffer = createFramebufferRenderBuffer(details.windowWidth, details.windowHeight);
	GLuint colorTexture = createFramebufferColorTexture(details.windowWidth, details.windowHeight);
	FBO = createFramebuffer(renderBuffer, colorTexture);

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)sizeof(glm::vec3));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)(2 * sizeof(glm::vec3)));
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &EBO);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * indices.size(), indices.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &UBO);
	glBindBuffer(GL_UNIFORM_BUFFER, UBO);
	glUniformBlockBinding(shaderProgram, 0, 0);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, UBO);
}

void setup() {
	initializeStates();
	initializeContext();
	createScene();
	setupGraphics();
}

void updateControls() {
	state.previousTime = state.currentTime;
	state.currentTime = std::chrono::high_resolution_clock::now();
	state.timeDelta = std::chrono::duration<double_t, std::chrono::seconds::period>(state.currentTime - state.previousTime).count();
	state.checkPoint += state.timeDelta;

	if (VR) {
		/*ovrTrackingState ts = ovr_GetTrackingState(session, ovr_GetTimeInSeconds(), ovrTrue);

		if (ts.StatusFlags & (ovrStatus_OrientationTracked | ovrStatus_PositionTracked))
		{
			ovrPosef pose = ts.HeadPose.ThePose;

			//std::cout << pose.Position.x << " " << pose.Position.y << " " << pose.Position.z << std::endl;
			//std::cout << pose.Orientation.w << " " << pose.Orientation.x << " " << pose.Orientation.y << " " << pose.Orientation.z << std::endl << std::endl;

			camera.previous = camera.position;
			camera.position = origin + glm::vec3{ -pose.Position.z + 0.5f, -pose.Position.x + 0.5f, pose.Position.y + 0.5f } *10.0f;

			glm::mat4 rotation = glm::toMat4(glm::qua{ pose.Orientation.w, pose.Orientation.z, -pose.Orientation.x, pose.Orientation.y });
			camera.direction = rotation * forward;
		}*/
	}

	else {
		auto moveDelta = state.timeDelta * 24.0, turnDelta = glm::radians(0.1);
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
	}

	auto replacement = camera.position - camera.previous;
	auto direction = glm::normalize(replacement);
	auto coefficient = 0.0f, distance = glm::length(replacement);

	for (auto& portal : portals) {
		if (epsilon < distance && glm::intersectRayPlane(camera.previous, direction, portal.mesh.origin, portal.direction, coefficient)) {
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

	std::cout << camera.room << std::endl;
}

bool visible(Portal& portal, Node& node) {
	if (portal.mesh.room == node.camera.room && portal.pairIndex != node.portalIndex)
		return true;
	else
		return false;
}

float sign(float value)
{
	if (value > 0.0f)
		return 1.0f;
	if (value < 0.0f)
		return -1.0f;
	return 0.0f;
}

glm::mat4 cullOblique(Portal& portal) {
	auto portalProjection = projection;
	auto plane = glm::vec4{ portal.direction, glm::dot(portal.direction, portal.mesh.origin) };

	glm::vec4 quaternion{};
	quaternion.x = (sign(plane.x) + portalProjection[2][0]) / portalProjection[0][0];
	quaternion.y = (sign(plane.y) + portalProjection[2][1]) / portalProjection[1][1];
	quaternion.z = -1.0f;
	quaternion.w = (1.0f + portalProjection[2][2]) / portalProjection[3][2];

	glm::vec4 clip = plane * (2.0f / glm::dot(plane, quaternion));

	portalProjection[0][2] = clip.x;
	portalProjection[1][2] = clip.y;
	portalProjection[2][2] = clip.z + 1.0F;
	portalProjection[3][2] = clip.w;

	return portalProjection;
}

void generateNodes() {
	nodes.clear();

	std::queue<Node> queue;

	auto transform = projection * glm::lookAt(camera.position, camera.position + camera.direction, camera.up);

	Node mainNode{ 0, -1, -1, camera, transform };

	queue.push(mainNode);
	nodes.push_back(mainNode);

	while (nodes.size() != details.maxCameraCount && !queue.empty()) {
		int32_t parentIndex = nodes.size() - queue.size();

		auto parentNode = queue.front();
		queue.pop();

		for (int32_t i = 0; i < portals.size(); i++) {
			auto& portal = portals.at(i);

			if (visible(portal, parentNode)) {
				Camera portalCamera {
					portal.targetRoom,
					portal.cameraTransform * glm::vec4{ parentNode.camera.position, 1.0f },
					portal.cameraTransform * glm::vec4{ parentNode.camera.direction, 0.0f },
					parentNode.camera.up,
					parentNode.camera.previous
				};

				auto portalProjection = projection;
				//auto portalProjection = cullOblique(portal);

				auto portalTransform = portalProjection * glm::lookAt(portalCamera.position,
					portalCamera.position + portalCamera.direction, portalCamera.up);

				Node portalNode{ parentNode.layer + 1, parentIndex, i, portalCamera, portalTransform };

				queue.push(portalNode);
				nodes.push_back(portalNode);

				if (nodes.size() == details.maxCameraCount)
					break;
			}
		}
	}
}

void drawMesh(Mesh& mesh) {
	glBindTexture(GL_TEXTURE_2D, textures.at(mesh.textureIndex).texture);
	glDrawElementsBaseVertex(GL_TRIANGLES, mesh.indexLength, GL_UNSIGNED_SHORT, (GLvoid*)(mesh.indexOffset * sizeof(GLushort)), mesh.vertexOffset);
}

void drawNodeView(uint8_t nodeIndex) {
	auto& node = nodes.at(nodeIndex);
	uint8_t mod = node.layer % 2;

	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), &node.transform, GL_DYNAMIC_DRAW);
	glClear(GL_DEPTH_BUFFER_BIT);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

	if (!mod) {
		uint8_t value = nodeIndex;

		glStencilFunc(GL_EQUAL, value, 0x0F);
		glStencilMask(0xF0);
	}

	else {
		uint8_t value = nodeIndex << 4;

		glStencilFunc(GL_EQUAL, value, 0xF0);
		glStencilMask(0x0F);
	}

	for (auto& mesh : meshes)
		drawMesh(mesh);

	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	for (uint8_t childIndex = nodeIndex + 1; childIndex < nodes.size(); childIndex++) {
		auto& childNode = nodes.at(childIndex);

		if (nodeIndex == childNode.parentIndex) {
			if (!mod) {
				uint8_t value = (childIndex << 4) + nodeIndex;

				glStencilFunc(GL_EQUAL, value, 0x0F);
				glStencilMask(0xF0);
			}

			else {
				uint8_t value = (nodeIndex << 4) + childIndex;

				glStencilFunc(GL_EQUAL, value, 0xF0);
				glStencilMask(0x0F);
			}

			auto& portalMesh = portals.at(childNode.portalIndex).mesh;
			drawMesh(portalMesh);
		}
	}
}

void drawViewport(GLint x, GLint y, GLint w, GLint h) {
	glStencilMask(0xFF);
	glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glViewport(x, y, w, h);

	for (uint8_t index = 0; index < nodes.size(); index++)
		drawNodeView(index);
}

void drawScene() {
	if (VR) {
		/*
		double displayMidpointSeconds = ovr_GetPredictedDisplayTime(session, 0);
		ovrTrackingState hmdState = ovr_GetTrackingState(session, displayMidpointSeconds, ovrTrue);
		ovr_CalcEyePoses(hmdState.HeadPose.ThePose, hmdToEyeViewPose, layer.RenderPose);
		*/
		ovrResult result;

		int imageIndex = -1;
		ovr_GetTextureSwapChainCurrentIndex(session, swapchain, &imageIndex);
		std::cout << "GetTextureSwapChainCurrentIndex: " << imageIndex << std::endl;
		
		result = ovr_WaitToBeginFrame(session, state.frameCount);
		std::cout << "WaitToBeginFrame: " << result << std::endl;

		auto& framebuffer = framebuffers.at(imageIndex);
		std::cout << "Framebuffer: " << framebuffer << std::endl;

		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

		glStencilMask(0xFF);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		result = ovr_BeginFrame(session, state.frameCount);
		std::cout << "BeginFrame: " << result << std::endl;
		/*
		for (int eye = 0; eye < 2; eye++) {
			Vector3f pos = originPos + originRot.Transform(layer.RenderPose[eye].Position);
			Matrix4f rot = originRot * Matrix4f(layer.RenderPose[eye].Orientation);

			Vector3f finalUp = rot.Transform(Vector3f(0, 1, 0));
			Vector3f finalForward = rot.Transform(Vector3f(0, 0, -1));
			Matrix4f view = Matrix4f::LookAtRH(pos, pos + finalForward, finalUp);

			Matrix4f proj = ovrMatrix4f_Projection(layer.Fov[eye], 0.2f, 1000.0f, 0);
		}
		*/

		drawViewport(layer.Viewport[0].Pos.x, layer.Viewport[0].Pos.y, layer.Viewport[0].Size.w, layer.Viewport[0].Size.h);
		drawViewport(layer.Viewport[1].Pos.x, layer.Viewport[1].Pos.y, layer.Viewport[1].Size.w, layer.Viewport[1].Size.h);

		ovr_CommitTextureSwapChain(session, swapchain);

		ovrLayerHeader* layers = &layer.Header;
		result = ovr_EndFrame(session, state.frameCount, nullptr, &layers, 1);
		std::cout << "EndFrame: " << result << std::endl << std::endl;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, FBO);
	drawViewport(0, 0, details.windowWidth, details.windowHeight);

	state.frameCount++;
}

void storePixels() {
	if (state.totalFrameCount < 1000)
		return;

	int depth = 3;
	int size = details.windowWidth * details.windowHeight * depth;
	uint8_t* pixels = static_cast<uint8_t*>(calloc(size, 1));

	glReadPixels(0, 0, details.windowWidth, details.windowHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);

	std::ofstream file("image.ppm");
	file << "P3\n" << details.windowWidth << " " << details.windowHeight << "\n255\n";

	for (int i = 0; i < size; i++) {
		file << (uint32_t)pixels[i] << " ";

		if(i % depth == depth - 1)
			file << "\n";
	}

	exit(0);
}

void updateFeedbacks() {
	if (state.checkPoint > 1.0) {
		state.totalFrameCount += state.frameCount;
		auto title = std::to_string(state.frameCount) + " - " + std::to_string(state.totalFrameCount);

		state.frameCount = 0;
		state.checkPoint = 0.0;
		glfwSetWindowTitle(window, title.c_str());
	}
}

void draw() {
	while (true) {
		glfwPollEvents();

		if (glfwWindowShouldClose(window))
			break;

		updateControls();
		generateNodes();
		drawScene();
		updateFeedbacks();

		//storePixels();
		glBlitNamedFramebuffer(FBO,
			0,
			0,
			0,
			details.windowWidth,
			details.windowHeight,
			0,
			0,
			details.windowWidth,
			details.windowHeight,
			GL_COLOR_BUFFER_BIT,
			GL_LINEAR);

		glfwSwapBuffers(window);

		if (state.frameCount == 5)
			exit(0);

		//std::this_thread::sleep_until(state.currentTime + std::chrono::milliseconds(1));
	}
}

void clean() {
	glDeleteFramebuffers(1, &FBO);

	glfwTerminate();

	if(VR)
		ovr_Destroy(session);
	
	ovr_Shutdown();
}

int main()
{
	setup();
	draw();
	clean();
}
