#pragma once
#include <vulkan/vulkan.h>
#include "renderer/PhysicalDevice.h"

struct MeshCreationObjects
{
	VkDevice logicalDevice;
	PhysicalDevice physicalDevice;
	VkCommandPool commandPool;
	VkQueue queue;
};