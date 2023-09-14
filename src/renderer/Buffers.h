#pragma once
#include <vulkan/vulkan.h>
#include "Vertex.h"
#include "PhysicalDevice.h"

class VulkanBuffer // maybe struct because its fairly small and used for the entire lifetime of the mesh?
{
public:
	void Destroy();
	VkBuffer GetVkBuffer();

protected:
	template<typename T> void GenerateBuffer(PhysicalDevice physicalDevice, VkBufferUsageFlags usage, VkCommandPool commandPool, VkQueue queue, const std::vector<T> vertices);

	VkDevice logicalDevice;
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;
};

class VertexBuffer : public VulkanBuffer
{
public:
	VertexBuffer() = default;
	VertexBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, const std::vector<Vertex> vertices); // maybe its better to get the vertices / indices via reference, since they can contain a lot of data and it all needs to be copied
};

class IndexBuffer : public VulkanBuffer
{
public:
	IndexBuffer() = default;
	IndexBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, const std::vector<uint16_t> indices);
};