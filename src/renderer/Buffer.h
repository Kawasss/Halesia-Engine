#pragma once
#include <vulkan/vulkan.h>

class Buffer
{
public:
	Buffer() = default;
	Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) { Init(size, usage, properties); }
	~Buffer() { Destroy(); }

	Buffer(const Buffer&)       = delete;
	Buffer& operator=(Buffer&&) = delete;

	void Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	void Destroy();

	VkBuffer Get() { return buffer; }
	
	template<typename T> 
	T* Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags flags = 0);
	void* Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags flags = 0);
	void Unmap();

	void Fill(VkCommandBuffer commandBuffer, uint32_t value = 0, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

	void SetDebugName(const char* name);

private:
	VkBuffer buffer       = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
};

template<typename T> 
T* Buffer::Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags) 
{ 
	return static_cast<T*>(Map(offset, size, flags)); 
}