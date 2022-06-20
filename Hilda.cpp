#include "Hilda.hpp"

GLFWwindow* window;

tinygltf::TinyGLTF objectLoader;

std::string assetFolder;
std::string shaderFolder;

ovrSession session;
ovrGraphicsLuid luid;
ovrHmdDesc hmdDesc;
ovrEyeRenderDesc eyeRenderDesc[2];
ovrPosef hmdToEyeViewPose[2];
ovrLayerEyeFov layer;

Framebuffer framebuffers[2];

ovrMirrorTexture mirrorTexture;
GLuint mirrorTextureBuffer;
GLuint mirrorFramebuffer;

uint32_t width;
uint32_t height;
uint32_t portalCount;
uint32_t meshCount;
uint32_t nodeLimit;

uint32_t currentImage;
uint32_t frameCount;
uint32_t totalFrameCount;
double_t timeDelta;
double_t checkPoint;

std::chrono::time_point<std::chrono::high_resolution_clock> previousTime;
std::chrono::time_point<std::chrono::high_resolution_clock> currentTime;

glm::vec3 previousPositions[2];
glm::vec3 currentPositions[2];
uint8_t currentRoom;

std::vector<GLushort> indices;
std::vector<Vertex> vertices;
std::vector<std::string> imageNames;
std::vector<Image> textures;
std::vector<Mesh> meshes;
std::vector<Portal> portals;
std::vector<Node> nodes;

GLuint VAO;
GLuint VBO;
GLuint EBO;
GLuint UBO;
GLuint shaderProgram;

//////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////

void keyboardCallback(GLFWwindow* handle, int key, int scancode, int action, int mods) {
	static_cast<void>(scancode);
	static_cast<void>(mods);

	if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
		glfwSetWindowShouldClose(handle, 1);
}

void resizeEvent(GLFWwindow* handle, int width, int height) {
	static_cast<void>(width);
	static_cast<void>(height);

	glfwSetWindowSize(handle, width, height);
}

//////////////////////////////////////////////////////////////////////////////

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
	for (auto& primitive : meshData.primitives) {
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
			meshCount++;
			meshes.push_back(mesh);
		}

		else if (type == Type::Portal) {
			Portal portal{};

			portal.mesh = mesh;
			portal.direction = glm::normalize(glm::vec3(mesh.transform * glm::vec4{ -1.0f, 0.0f, 0.0f, 0.0f }));

			portalCount++;
			portals.push_back(portal);
		}
	}
}

void loadModel(Type type, const std::string name, uint8_t room) {
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
		auto& node = model.nodes.front();
		currentRoom = room;
		currentPositions[0] = currentPositions[1] = getNodeTranslation(node) * glm::vec4{ 0.0f, 0.0f, 0.0f, 1.0f };
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
			loadMesh(model, mesh, type, getNodeTranslation(node), getNodeRotation(node), getNodeScale(node), room);
		}

		if (type == Type::Portal && portals.size() % 2 == 0) {
			auto& bluePortal = portals.at(portals.size() - 2);
			auto& orangePortal = portals.at(portals.size() - 1);

			bluePortal.targetRoom = orangePortal.mesh.room;
			orangePortal.targetRoom = bluePortal.mesh.room;

			bluePortal.translation = orangePortal.mesh.origin - bluePortal.mesh.origin;
			orangePortal.translation = bluePortal.mesh.origin - orangePortal.mesh.origin;
		}
	}
}

void createScene() {
	assetFolder = "Assets/backroom/";

	loadModel(Type::Camera, "c", 1);

	loadModel(Type::Portal, "p12", 1);
	loadModel(Type::Portal, "p21", 2);

	loadModel(Type::Mesh, "r1", 1);
	loadModel(Type::Mesh, "r2", 2);

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

	assetFolder = "Assets/backroom/";

	loadModel( Type::Camera, "camera", 1);

	loadModel( Type::Portal, "portal12", 1);
	//loadModel( Type::Portal, "portal21", 2);

	loadModel( Type::Mesh, "room1", 1);
	loadModel( Type::Mesh, "room2", 2);

	// TODO: Fix portal association
	assetFolder = "Assets/italy/";

	loadModel(Type::Camera, "camera", 2);

	loadModel(Type::Portal, "portal12", 1);
	loadModel(Type::Portal, "portal21", 2);
	loadModel(Type::Portal, "portal23", 2);
	loadModel(Type::Portal, "portal32", 3);
	loadModel(Type::Portal, "portal34", 3);
	loadModel(Type::Portal, "portal43", 4);
	loadModel(Type::Portal, "portal45", 4);
	loadModel(Type::Portal, "portal54", 5);
	loadModel(Type::Portal, "portal56", 5);
	loadModel(Type::Portal, "portal65", 6);
	loadModel(Type::Portal, "portal67", 6);
	loadModel(Type::Portal, "portal76", 7);
	loadModel(Type::Portal, "portal78", 7);
	loadModel(Type::Portal, "portal87", 8);
	loadModel(Type::Portal, "portal89", 8);
	loadModel(Type::Portal, "portal98", 9);
	loadModel(Type::Portal, "portal9A", 9);
	loadModel(Type::Portal, "portalA9", 10);
	loadModel(Type::Portal, "portalA1", 10);
	loadModel(Type::Portal, "portal1A", 1);

	loadModel(Type::Mesh, "room1", 1);
	loadModel(Type::Mesh, "room2", 2);
	loadModel(Type::Mesh, "room3", 3);
	loadModel(Type::Mesh, "room4", 4);
	loadModel(Type::Mesh, "room5", 5);
	loadModel(Type::Mesh, "room6", 6);
	loadModel(Type::Mesh, "room7", 7);
	loadModel(Type::Mesh, "room8", 8);
	loadModel(Type::Mesh, "room9", 9);
	loadModel(Type::Mesh, "roomA", 10);

	assetFolder = "Assets/italy/";

	loadModel(Type::Camera, "camera", 3);

	loadModel(Type::Portal, "portal23", 2);
	loadModel(Type::Portal, "portal32", 3);
	loadModel(Type::Portal, "portal34", 3);
	loadModel(Type::Portal, "portal43", 4);

	loadModel(Type::Mesh, "room2", 2);
	loadModel(Type::Mesh, "room3", 3);
	loadModel(Type::Mesh, "room4", 4);
	*/
}

//////////////////////////////////////////////////////////////////////////////

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

void setup() {
	ovr_Initialize(nullptr);
	ovr_Create(&session, &luid);

	hmdDesc = ovr_GetHmdDesc(session);

	width = hmdDesc.Resolution.w / 2;
	height = hmdDesc.Resolution.h / 2;

	nodeLimit = 15;	// 2 ^ 4 - 1 - 1

	currentImage = 0;
	frameCount = 0;
	totalFrameCount = 0;
	checkPoint = 0.0f;

	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	//glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);

	window = glfwCreateWindow(width, height, "", nullptr, nullptr);

	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, keyboardCallback);
	glfwSetFramebufferSizeCallback(window, resizeEvent);

	glfwSwapInterval(0);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	createScene();

	glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_STENCIL_TEST);
	glEnable(GL_SCISSOR_TEST);

	glFrontFace(GL_CCW);
	glCullFace(GL_BACK);

	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);

	glClearDepth(1.0f);
	glClearStencil(0);
	glClearColor(0.4f, 0.8f, 1.0f, 1.0f);

	for (int eye = 0; eye < 2; eye++) {
		auto& framebuffer = framebuffers[eye];

		ovrSizei idealTextureSize = ovr_GetFovTextureSize(session, ovrEyeType(eye), hmdDesc.DefaultEyeFov[eye], 1);

		framebuffer.width = idealTextureSize.w;
		framebuffer.height = idealTextureSize.h;

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = framebuffer.width;
		desc.Height = framebuffer.height;
		desc.MipLevels = 1;
		desc.SampleCount = 1;
		desc.StaticImage = ovrFalse;

		int length;

		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

		ovr_CreateTextureSwapChainGL(session, &desc, &framebuffer.textureSwapchain);
		ovr_GetTextureSwapChainLength(session, framebuffer.textureSwapchain, &length);

		for (int i = 0; i < length; i++)
		{
			GLuint textureIndex;
			ovr_GetTextureSwapChainBufferGL(session, framebuffer.textureSwapchain, i, &textureIndex);
			glBindTexture(GL_TEXTURE_2D, textureIndex);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		desc.Format = OVR_FORMAT_D32_FLOAT_S8X24_UINT;

		ovr_CreateTextureSwapChainGL(session, &desc, &framebuffer.depthStencilSwapchain);
		ovr_GetTextureSwapChainLength(session, framebuffer.depthStencilSwapchain, &length);

		for (int i = 0; i < length; ++i)
		{
			GLuint textureIndex;
			ovr_GetTextureSwapChainBufferGL(session, framebuffer.depthStencilSwapchain, i, &textureIndex);
			glBindTexture(GL_TEXTURE_2D, textureIndex);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

		glGenFramebuffers(1, &framebuffer.framebuffer);
	}

	ovrMirrorTextureDesc mirrorDescriptor{};
	mirrorDescriptor.Width = width;
	mirrorDescriptor.Height = width;
	mirrorDescriptor.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

	ovr_CreateMirrorTextureWithOptionsGL(session, &mirrorDescriptor, &mirrorTexture);
	ovr_GetMirrorTextureBufferGL(session, mirrorTexture, &mirrorTextureBuffer);

	glGenFramebuffers(1, &mirrorFramebuffer);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFramebuffer);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTextureBuffer, 0);
	glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	ovr_SetTrackingOriginType(session, ovrTrackingOrigin_FloorLevel);

	shaderFolder = "Shaders/";
	GLuint vertexShader = createShader("vertex.vert", GL_VERTEX_SHADER);
	GLuint fragmentShader = createShader("fragment.frag", GL_FRAGMENT_SHADER);
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

//////////////////////////////////////////////////////////////////////////////

bool visible(Portal& portal, Node& node) {
	if (node.room == portal.mesh.room && portal.pairIndex != node.portalIndex)
		return true;
	else
		return false;
}

void drawMesh(Mesh& mesh) {
	glBindTexture(GL_TEXTURE_2D, textures.at(mesh.textureIndex).texture);
	glDrawElementsBaseVertex(GL_TRIANGLES, mesh.indexLength, GL_UNSIGNED_SHORT, (GLvoid*)(mesh.indexOffset * sizeof(GLushort)), mesh.vertexOffset);
}

void drawNodeView(uint8_t nodeIndex) {
	auto& node = nodes.at(nodeIndex);
	uint8_t mod = node.layer % 2;

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
		if (node.room == mesh.room)
			drawMesh(mesh);

	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

	for (uint8_t childIndex = nodeIndex + 1; childIndex < nodes.size(); childIndex++) {
		auto& childNode = nodes.at(childIndex);
		auto& portal = portals.at(childNode.portalIndex);

		if (nodeIndex == childNode.parentIndex && portal.targetRoom == childNode.room) {
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

			drawMesh(portal.mesh);
		}
	}
}

void updateFeedbacks() {
	if (checkPoint > 1.0) {
		totalFrameCount += frameCount;
		auto title = std::to_string(frameCount) + " - " + std::to_string(totalFrameCount);

		frameCount = 0;
		checkPoint = 0.0;
		glfwSetWindowTitle(window, title.c_str());
	}
}

void draw() {
	float Yaw = 0;

	OVR::Vector3f Pos[2];
	Pos[0] = OVR::Vector3f(currentPositions[0].x, currentPositions[0].y, currentPositions[0].z);
	Pos[1] = OVR::Vector3f(currentPositions[1].x, currentPositions[1].y, currentPositions[1].z);

	while (true) {
		glfwPollEvents();

		if (glfwWindowShouldClose(window))
			break;

		ovrSessionStatus sessionStatus;
		ovr_GetSessionStatus(session, &sessionStatus);

		if (sessionStatus.ShouldQuit)
			break;

		if (sessionStatus.ShouldRecenter)
			ovr_RecenterTrackingOrigin(session);

		if (sessionStatus.IsVisible)
		{
			ovrEyeRenderDesc eyeRenderDesc[2];
			eyeRenderDesc[0] = ovr_GetRenderDesc(session, ovrEye_Left, hmdDesc.DefaultEyeFov[0]);
			eyeRenderDesc[1] = ovr_GetRenderDesc(session, ovrEye_Right, hmdDesc.DefaultEyeFov[1]);

			ovrPosef EyeRenderPose[2];
			ovrPosef HmdToEyePose[2] = { eyeRenderDesc[0].HmdToEyePose,
										 eyeRenderDesc[1].HmdToEyePose };

			double sensorSampleTime;
			ovr_GetEyePoses(session, frameCount, ovrTrue, HmdToEyePose, EyeRenderPose, &sensorSampleTime);

			ovrTimewarpProjectionDesc posTimewarpProjectionDesc = {};

			for (int eye = 0; eye < 2; eye++)
			{
				auto& framebuffer = framebuffers[eye];

				GLuint curColorTexId;
				GLuint curDepthTexId;

				int curIndex;

				ovr_GetTextureSwapChainCurrentIndex(session, framebuffer.textureSwapchain, &curIndex);
				ovr_GetTextureSwapChainBufferGL(session, framebuffer.textureSwapchain, curIndex, &curColorTexId);

				ovr_GetTextureSwapChainCurrentIndex(session, framebuffer.depthStencilSwapchain, &curIndex);
				ovr_GetTextureSwapChainBufferGL(session, framebuffer.depthStencilSwapchain, curIndex, &curDepthTexId);

				glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.framebuffer);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curColorTexId, 0);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, curDepthTexId, 0);

				glViewport(0, 0, framebuffer.width, framebuffer.height);
				glScissor(0, 0, framebuffer.width, framebuffer.height);

				glStencilMask(0xFF);
				glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

				OVR::Matrix4f rollPitchYaw = OVR::Matrix4f::RotationY(Yaw);
				OVR::Matrix4f finalRollPitchYaw = rollPitchYaw * OVR::Matrix4f(EyeRenderPose[eye].Orientation);
				OVR::Vector3f finalUp = finalRollPitchYaw.Transform(OVR::Vector3f(0, 1, 0));
				OVR::Vector3f finalForward = finalRollPitchYaw.Transform(OVR::Vector3f(0, 0, -1));
				OVR::Vector3f shiftedEyePos = Pos[eye] + rollPitchYaw.Transform(EyeRenderPose[eye].Position);

				previousPositions[eye] = currentPositions[eye];
				currentPositions[eye] = glm::vec3{ shiftedEyePos.x, shiftedEyePos.y, shiftedEyePos.z };

				auto replacement = currentPositions[eye] - previousPositions[eye];
				auto direction = glm::normalize(replacement);
				auto coefficient = 0.0f, distance = glm::length(replacement);

				for (auto& portal : portals) {
					if (epsilon < distance && glm::intersectRayPlane(previousPositions[eye], direction, portal.mesh.origin, portal.direction, coefficient)) {
						auto point = previousPositions[eye] + coefficient * direction;

						if (point.x >= portal.mesh.minBorders.x && point.y >= portal.mesh.minBorders.y && point.z >= portal.mesh.minBorders.z &&
							point.x <= portal.mesh.maxBorders.x && point.y <= portal.mesh.maxBorders.y && point.z <= portal.mesh.maxBorders.z &&
							0 <= coefficient && distance >= coefficient) {
							currentRoom = portal.targetRoom;

							currentPositions[eye] = currentPositions[eye] + portal.translation;
							previousPositions[eye] = currentPositions[eye];

							break;
						}
					}
				}

				nodes.clear();

				std::queue<Node> queue;

				Node mainNode{ 0, -1, -1, currentRoom, glm::vec3{shiftedEyePos.x, shiftedEyePos.y, shiftedEyePos.z} };

				queue.push(mainNode);
				nodes.push_back(mainNode);

				while (nodes.size() != nodeLimit && !queue.empty()) {
					int32_t parentIndex = nodes.size() - queue.size();

					auto parentNode = queue.front();
					queue.pop();

					for (int32_t i = 0; i < portals.size(); i++) {
						auto& portal = portals.at(i);

						if (visible(portal, parentNode)) {
							Node portalNode{ parentNode.layer + 1, parentIndex, i, portal.targetRoom, parentNode.translation + portal.translation };

							queue.push(portalNode);
							nodes.push_back(portalNode);

							if (nodes.size() == nodeLimit)
								break;
						}
					}
				}

				for (uint8_t index = 0; index < nodes.size(); index++) {
					auto& node = nodes.at(index);

					OVR::Vector3f nodeEyePos(node.translation.x, node.translation.y, node.translation.z);

					OVR::Matrix4f view = OVR::Matrix4f::LookAtRH(nodeEyePos, nodeEyePos + finalForward, finalUp);
					OVR::Matrix4f proj = ovrMatrix4f_Projection(hmdDesc.DefaultEyeFov[eye], 0.01f, 1000.0f, ovrProjection_None);
					posTimewarpProjectionDesc = ovrTimewarpProjectionDesc_FromProjection(proj, ovrProjection_None);

					auto transform = proj * view;
					transform.Transpose();

					glBufferData(GL_UNIFORM_BUFFER, sizeof(transform), (GLfloat*)&transform, GL_DYNAMIC_DRAW);

					drawNodeView(index);
				}

				glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.framebuffer);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

				ovr_CommitTextureSwapChain(session, framebuffer.textureSwapchain);
				ovr_CommitTextureSwapChain(session, framebuffer.depthStencilSwapchain);
			}

			ovrLayerEyeFovDepth ld{};
			ld.Header.Type = ovrLayerType_EyeFovDepth;
			ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
			ld.ProjectionDesc = posTimewarpProjectionDesc;
			ld.SensorSampleTime = sensorSampleTime;

			for (int eye = 0; eye < 2; eye++)
			{
				auto& framebuffer = framebuffers[eye];

				ld.ColorTexture[eye] = framebuffer.textureSwapchain;
				ld.DepthTexture[eye] = framebuffer.depthStencilSwapchain;
				ld.Viewport[eye] = OVR::Recti(OVR::Sizei(framebuffer.width, framebuffer.height));
				ld.Fov[eye] = hmdDesc.DefaultEyeFov[eye];
				ld.RenderPose[eye] = EyeRenderPose[eye];
			}

			ovrLayerHeader* layers = &ld.Header;
			ovr_SubmitFrame(session, frameCount, nullptr, &layers, 1);
		}

		glBindFramebuffer(GL_READ_FRAMEBUFFER, mirrorFramebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		glBlitFramebuffer(0, height, width, 0, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glfwSwapBuffers(window);

		frameCount++;
	}
}

void clean() {
	glfwTerminate();

	ovr_Destroy(session);
	ovr_Shutdown();
}

int main()
{
	setup();
	draw();
	clean();
}
