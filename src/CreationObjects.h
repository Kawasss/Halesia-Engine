#pragma once
#include <vulkan/vulkan.h>
#include "renderer/PhysicalDevice.h"

struct VulkanCreationObjects
{
	VkDevice logicalDevice;
	PhysicalDevice physicalDevice;
	VkCommandPool commandPool;
	VkQueue queue;
};
typedef VulkanCreationObjects TextureCreationObjects;
typedef VulkanCreationObjects MeshCreationObjects;