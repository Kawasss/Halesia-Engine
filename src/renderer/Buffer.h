#pragma once
#include <vulkan/vulkan.h>
#include <array>

#include "FramesInFlight.h"
#include "VideoMemoryManager.h"

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

	VkBuffer Get() { return buffer.Get(); }
	
	void InheritFrom(Buffer& parent); // inherits the members from the parent and tells the parent to not destroy its members upon destruction (the buffer will destroy the current members first)

	template<typename T> 
	T* Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags flags = 0);
	void* Map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags flags = 0);
	void Unmap();

	void Fill(VkCommandBuffer commandBuffer, uint32_t value = 0, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

	void SetDebugName(const char* name);

private:
	VvmBuffer buffer;
};

template<typename T> 
T* Buffer::Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags) 
{ 
	return static_cast<T*>(Map(offset, size, flags)); 
}

namespace FIF
{
	class Buffer
	{
	public:
		Buffer() = default;
		Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) { Init(size, usage, properties); }
		~Buffer() { Destroy(); }

		Buffer(const Buffer&) = delete;
		Buffer& operator=(Buffer&&) = delete;

		VkBuffer operator[](size_t index) const { return buffers[index].Get(); }

		VkBuffer Get() { return buffers[FIF::frameIndex].Get(); }

		void Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		void Destroy();

		void MapPermanently();
		void Unmap();

		template<typename T>
		T*    GetMappedPointer() { return static_cast<T*>(GetMappedPointer()); }
		void* GetMappedPointer() { return pointers[FIF::frameIndex]; }

		void InheritFrom(FIF::Buffer& parent); // inherits the members from the parent and tells the parent to not destroy its members upon destruction (the buffer will destroy the current members first)

		void SetDebugName(const char* name);

	private:
		std::array<VvmBuffer, FIF::FRAME_COUNT> buffers; // should initialize all values to VK_NULL_HANDLE
		std::array<void*, FIF::FRAME_COUNT> pointers;
	};
}