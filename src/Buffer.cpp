#include "renderer/Buffer.h"
#include "renderer/Vulkan.h"

void Buffer::Init(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
	Vulkan::CreateBuffer(size, usage, properties, buffer, memory);
}

void Buffer::Destroy()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	if (buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(ctx.logicalDevice, buffer, nullptr);
	if (memory != VK_NULL_HANDLE)
		vkFreeMemory(ctx.logicalDevice, memory, nullptr);
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