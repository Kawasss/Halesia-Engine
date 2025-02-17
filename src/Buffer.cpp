#include "renderer/Buffer.h"
#include "renderer/Vulkan.h"
#include "renderer/GarbageManager.h"
#include "renderer/VideoMemoryManager.h"

void Buffer::Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	buffer = Vulkan::CreateBuffer(size, usage, properties);
}

void Buffer::Destroy()
{
	if (!buffer.IsValid())
		return;

	const Vulkan::Context& ctx = Vulkan::GetContext();
	buffer.Destroy();

	buffer = VK_NULL_HANDLE;
}

void* Buffer::Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	return vvm::MapBuffer(buffer, offset, size, flags);
}

void Buffer::Unmap()
{
	vvm::UnmapBuffer(buffer);
}

void Buffer::InheritFrom(Buffer& other)
{
	Destroy();

	this->buffer = other.buffer;

	// setting all buffer members to a null handle disables the Destroy function
	other.buffer = VK_NULL_HANDLE;
}

VkDeviceAddress Buffer::GetDeviceAddress()
{
	return Vulkan::GetDeviceAddress(this->Get());
}

void Buffer::SetDebugName(const char* name)
{
	Vulkan::SetDebugName(buffer, name);
}

void Buffer::Fill(VkCommandBuffer commandBuffer, uint32_t value, VkDeviceSize offset, VkDeviceSize size)
{
	vkCmdFillBuffer(commandBuffer, buffer.Get(), offset, size, value);
}

namespace FIF
{
	void Buffer::Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	{
		for (unsigned int i = 0; i < FIF::FRAME_COUNT; i++)
			buffers[i] = Vulkan::CreateBuffer(size, usage, properties);
	}

	void Buffer::Destroy()
	{
		for (int i = 0; i < FIF::FRAME_COUNT; i++)
			buffers[i].Destroy();
	}

	void Buffer::InheritFrom(FIF::Buffer& parent)
	{
		Destroy();

		for (int i = 0; i < FIF::FRAME_COUNT; i++)
		{
			this->buffers[i] = parent.buffers[i];
			parent.buffers[i].Invalidate();
		}
	}

	void Buffer::MapPermanently()
	{
		for (int i = 0; i < FIF::FRAME_COUNT; i++)
			pointers[i] = vvm::MapBuffer(buffers[i]);
	}

	void Buffer::Unmap()
	{
		for (int i = 0; i < FIF::FRAME_COUNT; i++)
			vvm::UnmapBuffer(buffers[i]);
	}

	void Buffer::SetDebugName(const char* name)
	{
		for (int i = 0; i < FIF::FRAME_COUNT; i++)
			Vulkan::SetDebugName(buffers[i], name);
	}
}