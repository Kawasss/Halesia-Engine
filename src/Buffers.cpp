#include <vulkan/vulkan.h>
#include <vector>
#include "renderer/Vulkan.h"
#include "renderer/PhysicalDevice.h"
#include "renderer/Buffers.h"
#include "Vertex.h"
#include <iostream>

void VulkanBuffer::Destroy()
{
	vkDestroyBuffer(logicalDevice, buffer, nullptr);
	vkFreeMemory(logicalDevice, bufferMemory, nullptr);
	delete this;
}

VkBuffer VulkanBuffer::GetVkBuffer()
{
	return buffer;
}

template<typename T> void VulkanBuffer::GenerateBuffer(PhysicalDevice physicalDevice, VkBufferUsageFlags usage, VkCommandPool commandPool, VkQueue queue, const std::vector<T> bufferData)
{
	VkDeviceSize size = sizeof(bufferData[0]) * bufferData.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Vulkan::CreateBuffer(logicalDevice, physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, size, 0, &data);
	memcpy(data, bufferData.data(), (size_t)size);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);
	Vulkan::CopyBuffer(logicalDevice, commandPool, queue, stagingBuffer, buffer, size);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

VertexBuffer::VertexBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, const std::vector<Vertex> vertices)
{
	this->logicalDevice = logicalDevice;
	GenerateBuffer<Vertex>(physicalDevice, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, commandPool, queue, vertices);
}

IndexBuffer::IndexBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue, const std::vector<uint16_t> indices)
{
	this->logicalDevice = logicalDevice;
	GenerateBuffer<uint16_t>(physicalDevice, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, commandPool, queue, indices);
}