#include "renderer/Buffer.h"
#include "renderer/VideoMemoryManager.h"
#include "renderer/VulkanAPIError.h"

import Renderer.VulkanGarbageManager;
import Renderer.CommandBuffer;
import Renderer.Vulkan;

void Buffer::Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	buffer = Vulkan::CreateBuffer(size, usage, properties);
}

void Buffer::Destroy()
{
	if (buffer.IsValid())
		buffer.Destroy();
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
	Vulkan::SetDebugName(buffer.Get(), name);
}

void Buffer::Fill(VkCommandBuffer commandBuffer, uint32_t value, VkDeviceSize offset, VkDeviceSize size)
{
	vkCmdFillBuffer(commandBuffer, buffer.Get(), offset, size, value);
}

bool Buffer::IsValid() const
{
	return buffer.IsValid();
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

	void Buffer::Fill(const CommandBuffer& cmdBuffer, uint32_t value, VkDeviceSize offset, VkDeviceSize size)
	{
		for (int i = 0; i < FIF::FRAME_COUNT; i++)
			cmdBuffer.FillBuffer(buffers[i].Get(), offset, size, value);
	}

	bool Buffer::IsValid() const
	{
		for (int i = 0; i < FIF::FRAME_COUNT; i++)
			if (!buffers[i].IsValid())
				return false;
		return true;
	}
}

void ImmediateBuffer::Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	Vulkan::CreateBufferHandle(buffer, size, usage);
	
	VkMemoryRequirements requirements{};
	vkGetBufferMemoryRequirements(ctx.logicalDevice, buffer, &requirements);

	Vulkan::AllocateMemory(memory, requirements, properties);
	vkBindBufferMemory(ctx.logicalDevice, buffer, memory, 0);
}

void ImmediateBuffer::Destroy()
{
	if (!IsValid())
		return;

	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkDestroyBuffer(ctx.logicalDevice, buffer, nullptr);
	vkFreeMemory(ctx.logicalDevice, memory, nullptr);

	Invalidate();
}

void ImmediateBuffer::InheritFrom(ImmediateBuffer& parent)
{
	buffer = parent.buffer;
	memory = parent.memory;

	parent.Invalidate();
}

void* ImmediateBuffer::Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	void* ret = nullptr;
	VkResult res = vkMapMemory(Vulkan::GetContext().logicalDevice, memory, offset, size, flags, &ret);
	CheckVulkanResult("Failed to map immediate buffer memory", res);

	return ret;
}

void ImmediateBuffer::Unmap()
{
	vkUnmapMemory(Vulkan::GetContext().logicalDevice, memory);
}

void ImmediateBuffer::SetDebugName(const char* name)
{
	Vulkan::SetDebugName(buffer, name);
}

bool ImmediateBuffer::IsValid() const
{
	return buffer != VK_NULL_HANDLE && memory != VK_NULL_HANDLE;
}

void ImmediateBuffer::Invalidate()
{
	buffer = VK_NULL_HANDLE;
	memory = VK_NULL_HANDLE;
}