#pragma once
#include <vulkan/vulkan.h>
#include "renderer/PhysicalDevice.h"

struct VulkanCreationObject
{
	VkDevice logicalDevice;
	PhysicalDevice physicalDevice;
	VkCommandPool commandPool;
	VkQueue queue;
};
typedef VulkanCreationObject TextureCreationObject;
typedef VulkanCreationObject MeshCreationObject;
typedef VulkanCreationObject ObjectCreationObject;
typedef VulkanCreationObject BufferCreationObject;