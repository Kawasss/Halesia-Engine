#include "renderer/Buffer.h"
#include "renderer/Vulkan.h"

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
		vkDestroyBuffer(ctx.logicalDevice, buffer, nullptr);
	if (memory != VK_NULL_HANDLE)
		vkFreeMemory(ctx.logicalDevice, memory, nullptr);

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