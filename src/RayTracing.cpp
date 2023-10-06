#include "renderer/RayTracing.h"
#include "renderer/Vulkan.h"

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 1;

VkCommandPool commandPool;
std::vector<VkCommandBuffer> commandBuffers(MAX_FRAMES_IN_FLIGHT);
VkDescriptorPool descriptorPool;

void RayTracing::Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface)
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{};
	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rayTracingProperties;

	vkGetPhysicalDeviceProperties2(physicalDevice.Device(), &properties2);

	// command pool

	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = physicalDevice.QueueFamilies(surface).graphicsFamily.value();

	if (!vkCreateCommandPool(logicalDevice, &commandPoolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a command pool for ray tracing");

	// command buffer

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

	if (!vkAllocateCommandBuffers(logicalDevice, &commandBufferAllocateInfo, commandBuffers.data()) != VK_SUCCESS)
		throw std::runtime_error("Failed to allocate the command buffers for ray tracing");

	// descriptor pool (frames in flight not implemented)

	std::vector<VkDescriptorPoolSize> descriptorPoolSizes(4);
	descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	descriptorPoolSizes[0].descriptorCount = 1;
	descriptorPoolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSizes[1].descriptorCount = 1;
	descriptorPoolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorPoolSizes[2].descriptorCount = 4;
	descriptorPoolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	descriptorPoolSizes[3].descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCreateInfo.maxSets = 2;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

	if (!vkCreateDescriptorPool(logicalDevice, &descriptorPoolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		throw std::runtime_error("Failed to create the descriptor pool for ray tracing");

	// descriptor set layout (frames in flight not implemented)
}