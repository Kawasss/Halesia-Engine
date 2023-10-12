#include <vulkan/vulkan.h>
#include <vector>
#include "renderer/Vulkan.h"
#include "renderer/PhysicalDevice.h"
#include "renderer/Buffers.h"
#include "Vertex.h"
#include <iostream>

void VulkanBuffer::Destroy()
{
	vkDeviceWaitIdle(logicalDevice);
	Vulkan::globalThreadingMutex->lock();
	vkDestroyBuffer(logicalDevice, buffer, nullptr);
	vkFreeMemory(logicalDevice, bufferMemory, nullptr);
	Vulkan::globalThreadingMutex->unlock();
	//delete this;
}

VkBuffer VulkanBuffer::GetVkBuffer()
{
	return buffer;
}

VertexBuffer::VertexBuffer(const BufferCreationObject& creationObject, const std::vector<Vertex> vertices)
{
	this->logicalDevice = creationObject.logicalDevice;
	GenerateBuffer<Vertex>(creationObject, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, vertices);
}

IndexBuffer::IndexBuffer(const BufferCreationObject& creationObject, const std::vector<uint16_t> indices)
{
	this->logicalDevice = creationObject.logicalDevice;
	GenerateBuffer<uint16_t>(creationObject, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, indices);
}