#pragma once
#include "renderer/PhysicalDevice.h"
#include "../Object.h"
#include "../Camera.h"
#include "Texture.h"

class RayTracing
{
public:
	void Destroy(VkDevice logicalDevice);
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Object* object, Camera* camera);
	void DrawFrame(Win32Window* window, Camera* camera);

private:
	VkDevice logicalDevice;
	VkCommandPool commandPool;
	//Image* image;
	PhysicalDevice physicalDevice;
};