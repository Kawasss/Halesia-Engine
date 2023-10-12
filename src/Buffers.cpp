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

//template<typename t> void vulkanbuffer::generatebuffer(buffercreationobject creationobject, vkbufferusageflags usage, const std::vector<t>& bufferdata)
//{
//	vulkan::globalthreadingmutex->lock();
//
//	vkdevicesize size = sizeof(bufferdata[0]) * bufferdata.size();
//
//	vkbuffer stagingbuffer;
//	vkdevicememory stagingbuffermemory;
//	
//	vulkan::createbuffer(creationobject.logicaldevice, creationobject.physicaldevice, size, vk_buffer_usage_transfer_src_bit, vk_memory_property_host_visible_bit | vk_memory_property_host_coherent_bit, stagingbuffer, stagingbuffermemory);
//
//	void* data;
//	vkmapmemory(logicaldevice, stagingbuffermemory, 0, size, 0, &data);
//	memcpy(data, bufferdata.data(), (size_t)size);
//	vkunmapmemory(logicaldevice, stagingbuffermemory);
//
//	vulkan::createbuffer(logicaldevice, creationobject.physicaldevice, size, usage, vk_memory_property_device_local_bit, buffer, buffermemory);
//	vulkan::copybuffer(logicaldevice, creationobject.commandpool, creationobject.queue, stagingbuffer, buffer, size);
//
//	vkdestroybuffer(logicaldevice, stagingbuffer, nullptr);
//	vkfreememory(logicaldevice, stagingbuffermemory, nullptr);
//
//	vulkan::globalthreadingmutex->unlock();
//}
//
//template<typename T> VulkanBuffer::VulkanBuffer(BufferCreationObject creationObject, VkBufferUsageFlags usage, const std::vector<T>& bufferData) 
//{ 
//	GenerateBuffer<T>(creationObject, usage, bufferData); 
//}

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