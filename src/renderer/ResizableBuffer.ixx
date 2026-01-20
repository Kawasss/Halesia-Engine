module;

#include <Windows.h>
#include <vulkan/vulkan.h>

#include "Buffer.h"
#include "CommandBuffer.h"

export module Renderer.ResizableBuffer;

import std;

export class ResizableBuffer // everything is in bytes
{
public:
	enum class MemoryType
	{
		None,
		Cpu,
		Gpu,
	};

	ResizableBuffer() = default;
	~ResizableBuffer();

	void Destroy();

	void Init(size_t size, MemoryType memoryType, VkBufferUsageFlags usage); // will automatically add VK_BUFFER_USAGE_TRANSFER_DST etc. if needed

	void Write(const void* pValues, size_t writeSize, size_t offset);
	void Resize(size_t newSize);

	void Fill(const CommandBuffer& cmdBuffer, uint32_t value, size_t writeSize, size_t offset);
	void Fill(uint32_t value, size_t writeSize, size_t offset); // uses single time commands

	template<typename T>
	void Write(const std::span<T>& values, size_t offset) // offset in byte count
	{
		Write(values.data(), values.size() * sizeof(T), offset);
	}

	bool Resized();

	VkBuffer Get()   const { return buffer.Get(); }
	size_t GetSize() const { return size; }

	bool IsValid() const { return buffer.IsValid(); }

private:
	void ResizeHost(size_t newSize);
	void ResizeDevice(size_t newSize);

	// already expects the buffer to appropiately resized
	void WriteHost(const void* pValues, size_t writeSize, size_t offset);
	void WriteDevice(const void* pValues, size_t writeSize, size_t offset);

	Buffer transferBuffer;
	Buffer buffer;
	size_t size = 0;
	MemoryType memoryType = MemoryType::None;

	VkBufferUsageFlags usage = 0;
	VkMemoryPropertyFlags properties = 0;

	VkDeviceSize transferSize = 0;

	bool resized = false;
};