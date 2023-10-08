#pragma once
#include "renderer/PhysicalDevice.h"
#include "../Object.h"

class RayTracing
{
public:
	void Destroy(VkDevice logicalDevice);
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Object* object);

private:
	
};