#pragma once
#include "renderer/Vulkan.h"
#include "Vertex.h"
#include "PhysicalDevice.h"
#include "../CreationObjects.h"

class VulkanBuffer // maybe struct because its fairly small and used for the entire lifetime of the mesh?
{
public:
	VulkanBuffer() = default;
	template<typename T> VulkanBuffer(const BufferCreationObject& creationObject, VkBufferUsageFlags usage, const std::vector<T>& bufferData)
	{
		this->logicalDevice = creationObject.logicalDevice;
		GenerateBuffer<T>(creationObject, usage, bufferData);
	}

	void Destroy();
	VkBuffer GetVkBuffer();

protected:
	template<typename T> void GenerateBuffer(const BufferCreationObject& creationObject, VkBufferUsageFlags usage, const std::vector<T> bufferData)
	{
		VkDeviceSize size = sizeof(bufferData[0]) * bufferData.size();
		
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		
		void* data;
		vkMapMemory(creationObject.logicalDevice, stagingBufferMemory, 0, size, 0, &data);
		memcpy(data, bufferData.data(), (size_t)size);
		vkUnmapMemory(creationObject.logicalDevice, stagingBufferMemory);

		VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

		Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);
		Vulkan::CopyBuffer(creationObject.logicalDevice, commandPool/*creationObject.commandPool*/, creationObject.queue, stagingBuffer, buffer, size);
		
		vkDestroyBuffer(creationObject.logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(creationObject.logicalDevice, stagingBufferMemory, nullptr);

		Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);
	}

	VkDevice logicalDevice;
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;
};

class VertexBuffer : public VulkanBuffer
{
public:
	size_t size;

	VertexBuffer() = default;
	VertexBuffer(const BufferCreationObject& creationObject, const std::vector<Vertex> vertices); // maybe its better to get the vertices / indices via reference, since they can contain a lot of data and it all needs to be copied
};

class IndexBuffer : public VulkanBuffer
{
public:
	IndexBuffer() = default;
	IndexBuffer(const BufferCreationObject& creationObject, const std::vector<uint16_t> indices);
};