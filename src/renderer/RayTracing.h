#pragma once
#include "renderer/PhysicalDevice.h"
#include "../Object.h"
#include "../Camera.h"

class RayTracing
{
public:
	void Destroy(VkDevice logicalDevice);
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Object* object, Camera* camera);

private:
	
};