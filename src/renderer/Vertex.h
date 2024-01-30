#pragma once
#include <vulkan/vulkan.h>
#include <array>

#include "glm.h"

struct Vertex
{
	alignas(16) glm::vec3 position{};
	alignas(16) glm::vec3 normal{};
	alignas(16) glm::vec2 textureCoordinates{}; // alignas(8) doesnt get respected

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};

		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> descriptions{};

		descriptions[0].binding = 0;
		descriptions[0].location = 0;
		descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		descriptions[0].offset = offsetof(Vertex, position);

		descriptions[1].binding = 0;
		descriptions[1].location = 1;
		descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		descriptions[1].offset = offsetof(Vertex, normal);

		descriptions[2].binding = 0;
		descriptions[2].location = 2;
		descriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		descriptions[2].offset = offsetof(Vertex, textureCoordinates);

		return descriptions;
	}

	bool operator==(const Vertex& vert2)
	{
		return position == vert2.position && textureCoordinates == vert2.textureCoordinates && normal == vert2.normal;
	}
};