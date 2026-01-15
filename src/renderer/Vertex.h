#pragma once
#include <vulkan/vulkan.h>
#include <array>

#include "../glm.h"

constexpr uint32_t MAX_BONES_PER_VERTEX = 4;

struct Vertex
{
	Vertex() = default;
	Vertex(glm::vec3 pos) : position(pos) {}

	glm::vec3 position{};
	glm::vec3 normal{};
	glm::vec2 textureCoordinates{};
	glm::vec3 tangent{};
	glm::vec3 biTangent{};
	int boneIndices[MAX_BONES_PER_VERTEX] = { -1, -1, -1, -1 };
	float boneWeights[MAX_BONES_PER_VERTEX] = { 0.0f };

	static VkVertexInputBindingDescription GetBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription{};

		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 7> GetAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 7> descriptions{};

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

		descriptions[3].binding = 0;
		descriptions[3].location = 3;
		descriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
		descriptions[3].offset = offsetof(Vertex, tangent);

		descriptions[4].binding = 0;
		descriptions[4].location = 4;
		descriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
		descriptions[4].offset = offsetof(Vertex, biTangent);

		descriptions[5].binding = 0;
		descriptions[5].location = 5;
		descriptions[5].format = VK_FORMAT_R32G32B32A32_SINT;
		descriptions[5].offset = offsetof(Vertex, boneIndices);

		descriptions[6].binding = 0;
		descriptions[6].location = 6;
		descriptions[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		descriptions[6].offset = offsetof(Vertex, boneWeights);

		return descriptions;
	}

	bool operator==(const Vertex& vert2) const
	{
		return position == vert2.position && textureCoordinates == vert2.textureCoordinates && normal == vert2.normal;
	}
};