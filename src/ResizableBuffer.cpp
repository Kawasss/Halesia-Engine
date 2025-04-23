#include "renderer/ResizableBuffer.h"
#include "renderer/Vulkan.h"

static VkMemoryPropertyFlags GetMemoryPropertyFlags(ResizableBuffer::MemoryType memoryType)
{
	switch (memoryType)
	{
	case ResizableBuffer::MemoryType::Cpu:
		return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	case ResizableBuffer::MemoryType::Gpu:
		return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}
	return 0;
}

static VkBufferUsageFlags GetBufferUsageFlags(ResizableBuffer::MemoryType memoryType)
{
	switch (memoryType)
	{
	case ResizableBuffer::MemoryType::Cpu:
	case ResizableBuffer::MemoryType::Gpu:
		return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}
	return 0;
}

static size_t MultiplyUntilBiggerThan(size_t curr, size_t needed)
{
	size_t multiplier = 2;
	while (curr * multiplier < needed)
		multiplier *= 2;

	return curr * multiplier;
}

ResizableBuffer::~ResizableBuffer()
{
	Destroy();
}

void ResizableBuffer::Destroy()
{
	buffer.~Buffer();
}

void ResizableBuffer::Init(size_t size, MemoryType memoryType, VkBufferUsageFlags usage)
{
	properties = GetMemoryPropertyFlags(memoryType);
	this->usage = usage | GetBufferUsageFlags(memoryType);
	this->memoryType = memoryType;
	this->size = size;

	buffer.Init(size, this->usage, properties);
}

void ResizableBuffer::Write(const void* pValues, size_t writeSize, size_t offset)
{
	if (offset + writeSize > size)
	{
		size_t newSize = MultiplyUntilBiggerThan(size, offset + writeSize);
		Resize(newSize);
	}

	if (memoryType == MemoryType::Gpu)
		WriteDevice(pValues, writeSize, offset);
	else
		WriteHost(pValues, writeSize, offset);
}

void ResizableBuffer::Resize(size_t newSize)
{
	if (size == newSize)
		return;

	if (memoryType == MemoryType::Gpu)
		ResizeDevice(newSize);
	else
		ResizeHost(newSize);
}

void ResizableBuffer::ResizeDevice(size_t newSize)
{
	Buffer newBuffer(newSize, usage, properties);

	Vulkan::ExecuteSingleTimeCommands(
		[&](const CommandBuffer& cmdBuffer)
		{
			VkBufferCopy copy{};
			copy.dstOffset = 0;
			copy.srcOffset = 0;
			copy.size = size;

			cmdBuffer.CopyBuffer(buffer.Get(), newBuffer.Get(), 1, &copy);
		}
	);

	size = newSize;
	resized = true;

	buffer.InheritFrom(newBuffer);
}

void ResizableBuffer::ResizeHost(size_t newSize)
{
	Buffer newBuffer(newSize, usage, properties);

	void* src = buffer.Map();
	void* dst = newBuffer.Map();

	std::memcpy(dst, src, size);

	size = newSize;
	resized = true;

	buffer.InheritFrom(newBuffer);
}

void ResizableBuffer::WriteDevice(const void* pValues, size_t writeSize, size_t offset)
{
	Buffer transferBuffer(writeSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	void* dst = transferBuffer.Map();

	std::memcpy(dst, pValues, writeSize);

	Vulkan::ExecuteSingleTimeCommands(
		[&](const CommandBuffer& cmdBuffer)
		{
			VkBufferCopy copy{};
			copy.srcOffset = 0;
			copy.dstOffset = offset;
			copy.size = writeSize;

			cmdBuffer.CopyBuffer(transferBuffer.Get(), buffer.Get(), 1, &copy);
		}
	);
}

void ResizableBuffer::WriteHost(const void* pValues, size_t writeSize, size_t offset)
{
	void* dst = buffer.Map(offset, writeSize);
	std::memcpy(dst, pValues, writeSize);
	
	buffer.Unmap();
}

void ResizableBuffer::Fill(const CommandBuffer& cmdBuffer, uint32_t value, size_t writeSize, size_t offset)
{
	cmdBuffer.FillBuffer(buffer.Get(), offset, writeSize, value);
}

bool ResizableBuffer::Resized()
{
	bool ret = resized;
	resized = false;
	return ret;
}