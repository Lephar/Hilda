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
#include <LibOVR/Extras/OVR_Math.h>

constexpr auto epsilon = 0.0009765625f;

enum class Type {
	Mesh,
	Portal,
	Camera
};

struct Framebuffer {
	uint32_t width;
	uint32_t height;
	GLuint framebuffer;
	ovrTextureSwapChain depthStencilSwapchain;
	ovrTextureSwapChain textureSwapchain;
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texture;
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
	glm::vec3 translation;
};

struct Node {
	uint32_t layer;
	int32_t parentIndex;
	int32_t portalIndex;

	uint8_t room;
	glm::vec3 translation;
};
