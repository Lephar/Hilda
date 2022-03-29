#pragma once

#define TINYGLTF_USE_CPP14
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_IMPLEMENTATION

#define STBI_ONLY_JPEG
#define STBI_NO_FAILURE_STRINGS
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <array>
#include <string>
#include <limits>
#include <optional>
#include <vector>
#include <queue>
#include <chrono>
#include <memory>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <thread>
#include <mutex>
#include <semaphore>
#include <shared_mutex>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/intersect.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <tinygltf/tiny_gltf.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <LibOVR/OVR_CAPI.h>
#include <LibOVR/OVR_CAPI_GL.h>

constexpr auto epsilon = 0.0009765625f;

enum class Type {
	Mesh,
	Portal,
	Camera
};

struct Controls {
	bool keyW;
	bool keyA;
	bool keyS;
	bool keyD;

	double_t mouseX;
	double_t mouseY;
	double_t deltaX;
	double_t deltaY;
};

struct Details {
	uint32_t windowWidth;
	uint32_t windowHeight;
	uint32_t headsetWidth;
	uint32_t headsetHeight;
	int32_t headsetImageCount;
	uint32_t portalCount;
	uint32_t meshCount;
	uint32_t maxCameraCount;
};

struct State {
	uint32_t currentImage;
	uint32_t frameCount;
	uint32_t totalFrameCount;
	double_t timeDelta;
	double_t checkPoint;
	std::chrono::time_point<std::chrono::high_resolution_clock> previousTime;
	std::chrono::time_point<std::chrono::high_resolution_clock> currentTime;
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texture;
};

struct Camera {
	uint32_t room;

	glm::vec3 position;
	glm::vec3 direction;
	glm::vec3 up;

	glm::vec3 previous;
};

struct Image {
	int32_t width;
	int32_t height;
	int32_t channel;
	uint32_t texture;
};

struct Mesh {
	uint32_t indexOffset;
	uint32_t indexLength;
	uint32_t vertexOffset;
	uint32_t vertexLength;

	uint32_t textureIndex;
	std::string textureName;

	glm::vec3 origin;
	glm::vec3 minBorders;
	glm::vec3 maxBorders;

	uint8_t room;
	glm::mat4 transform;
};

struct Portal {
	Mesh mesh;
	uint8_t pairIndex;
	uint8_t targetRoom;

	glm::vec3 direction;
	glm::mat4 cameraTransform;
};

struct Node {
	uint32_t layer;
	int32_t parentIndex;
	int32_t portalIndex;

	Camera camera;
	glm::mat4 transform;
};
