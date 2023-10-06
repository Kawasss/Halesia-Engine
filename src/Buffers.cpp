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

template<typename T> void VulkanBuffer::GenerateBuffer(BufferCreationObject creationObject, VkBufferUsageFlags usage, const std::vector<T> bufferData)
{
	Vulkan::globalThreadingMutex->lock();

	VkDeviceSize size = sizeof(bufferData[0]) * bufferData.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, size, 0, &data);
	memcpy(data, bufferData.data(), (size_t)size);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	Vulkan::CreateBuffer(logicalDevice, creationObject.physicalDevice, size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);
	Vulkan::CopyBuffer(logicalDevice, creationObject.commandPool, creationObject.queue, stagingBuffer, buffer, size);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	Vulkan::globalThreadingMutex->unlock();
}

VertexBuffer::VertexBuffer(const BufferCreationObject& creationObject, const std::vector<Vertex> vertices)
{
	this->logicalDevice = creationObject.logicalDevice;
	GenerateBuffer<Vertex>(creationObject, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
}

IndexBuffer::IndexBuffer(const BufferCreationObject& creationObject, const std::vector<uint16_t> indices)
{
	this->logicalDevice = creationObject.logicalDevice;
	GenerateBuffer<uint16_t>(creationObject, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices);
}