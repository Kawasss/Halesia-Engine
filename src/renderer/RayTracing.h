#pragma once
#include "renderer/PhysicalDevice.h"

class RayTracing
{
private:
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface);
};