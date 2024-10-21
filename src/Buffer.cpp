#include "renderer/Buffer.h"
#include "renderer/Vulkan.h"
#include "renderer/GarbageManager.h"

void Buffer::Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	Vulkan::CreateBuffer(size, usage, properties, buffer, memory);
}

void Buffer::Destroy()
{
	if (buffer == VK_NULL_HANDLE && memory == VK_NULL_HANDLE)
		return;

	const Vulkan::Context& ctx = Vulkan::GetContext();

	if (buffer != VK_NULL_HANDLE)
		vgm::Delete(buffer);
	if (memory != VK_NULL_HANDLE)
		vgm::Delete(memory);

	buffer = VK_NULL_HANDLE;
	memory = VK_NULL_HANDLE;
}

void* Buffer::Map(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	
	void* ret = nullptr;
	VkResult result = vkMapMemory(ctx.logicalDevice, memory, offset, size, flags, &ret);
	CheckVulkanResult("Failed to map a buffers memory", result, vkMapMemory);

	return ret;
}

void Buffer::Unmap()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkUnmapMemory(ctx.logicalDevice, memory);
}

void Buffer::InheritFrom(Buffer& other)
{
	Destroy();

	this->buffer = other.buffer;
	this->memory = other.memory;

	// setting all buffer members to a null handle disables the Destroy function
	other.buffer = VK_NULL_HANDLE;
	other.memory = VK_NULL_HANDLE;
}

void Buffer::SetDebugName(const char* name)
{
	Vulkan::SetDebugName(buffer, name);
}

void Buffer::Fill(VkCommandBuffer commandBuffer, uint32_t value, VkDeviceSize offset, VkDeviceSize size)
{
	vkCmdFillBuffer(commandBuffer, buffer, offset, size, value);
}

namespace FIF
{
	void Buffer::Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	{
		for (unsigned int i = 0; i < FIF::FRAME_COUNT; i++)
			Vulkan::CreateBuffer(size, usage, properties, buffers[i], memories[i]);
	}

	void Buffer::Destroy()
	{
		const Vulkan::Context& ctx = Vulkan::GetContext();

		for (int i = 0; i < FIF::FRAME_COUNT; i++)
		{
			if (buffers[i] != VK_NULL_HANDLE)
			{
				vgm::Delete(buffers[i]);
				buffers[i] = VK_NULL_HANDLE;
			}
			if (memories[i] != VK_NULL_HANDLE)
			{
				vgm::Delete(memories[i]);
				memories[i] = VK_NULL_HANDLE;
			}
		}
	}

	void Buffer::InheritFrom(FIF::Buffer& parent)
	{
		Destroy();

		for (int i = 0; i < FIF::FRAME_COUNT; i++)
		{
			this->buffers[i] = parent.buffers[i];
			parent.buffers[i] = VK_NULL_HANDLE;

			this->memories[i] = parent.memories[i];
			parent.memories[i] = VK_NULL_HANDLE;
		}
	}

	void Buffer::MapPermanently()
	{
		const Vulkan::Context& ctx = Vulkan::GetContext();

		for (int i = 0; i < FIF::FRAME_COUNT; i++)
		{
			VkResult result = vkMapMemory(ctx.logicalDevice, memories[i], 0, VK_WHOLE_SIZE, 0, &pointers[i]);
			CheckVulkanResult("Failed to map a FIF buffer", result, vkMapMemory);
		}
	}

	void Buffer::Unmap()
	{
		const Vulkan::Context& ctx = Vulkan::GetContext();

		for (int i = 0; i < FIF::FRAME_COUNT; i++)
			vkUnmapMemory(ctx.logicalDevice, memories[i]);
	}

	void Buffer::SetDebugName(const char* name)
	{
		for (int i = 0; i < FIF::FRAME_COUNT; i++)
			Vulkan::SetDebugName(buffers[i], name);
	}
}