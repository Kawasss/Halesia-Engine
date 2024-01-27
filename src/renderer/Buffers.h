#pragma once
#include "renderer/Vulkan.h"
#include "Vertex.h"

template<typename T> class VulkanBuffer
{
public:
	VulkanBuffer() = default;
	VulkanBuffer(VkBufferUsageFlags usage, const std::vector<T>& bufferData)
	{
		this->logicalDevice = Vulkan::GetContext().logicalDevice;
		GenerateBuffer<T>(usage, bufferData);
	}

	void Destroy()
	{
		vkDestroyBuffer(logicalDevice, buffer, nullptr);
		vkFreeMemory(logicalDevice, bufferMemory, nullptr);
		//delete this;
	}

	VkBuffer GetVkBuffer() { return buffer; }

protected:
	template<typename T> void GenerateBuffer(VkBufferUsageFlags usage, const std::vector<T> bufferData)
	{
		const Vulkan::Context& context = Vulkan::GetContext();
		VkDeviceSize size = sizeof(bufferData[0]) * bufferData.size();
		
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
		
		void* data;
		vkMapMemory(context.logicalDevice, stagingBufferMemory, 0, size, 0, &data);
		memcpy(data, bufferData.data(), (size_t)size);
		vkUnmapMemory(context.logicalDevice, stagingBufferMemory);

		VkCommandPool commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);

		Vulkan::CreateBuffer(size, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);
		Vulkan::CopyBuffer(commandPool, context.graphicsQueue, stagingBuffer, buffer, size);
		
		vkDestroyBuffer(context.logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(context.logicalDevice, stagingBufferMemory, nullptr);

		Vulkan::YieldCommandPool(context.graphicsIndex, commandPool);
	}

	VkDevice logicalDevice;
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;
};

class VertexBuffer : public VulkanBuffer<Vertex>
{
public:
	size_t size;

	VertexBuffer() = default;
	VertexBuffer(const std::vector<Vertex> vertices); // maybe its better to get the vertices / indices via reference, since they can contain a lot of data and it all needs to be copied
};

class IndexBuffer : public VulkanBuffer<uint16_t>
{
public:
	IndexBuffer() = default;
	IndexBuffer(const std::vector<uint16_t> indices);
};