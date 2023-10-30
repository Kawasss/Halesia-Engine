#pragma once
#include "renderer/PhysicalDevice.h"
#include "../Object.h"
#include "../Camera.h"
#include "Texture.h"
#include "Swapchain.h"
#include "AccelerationStructures.h"
#include "../ResourceManager.h"

class RayTracing
{
public:
	RayTracing() {}
	void Destroy();
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Object* object, Camera* camera, Win32Window* window, Swapchain* swapchain);
	void DrawFrame(std::vector<Object*> objects, Win32Window* window, Camera* camera, VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void RecreateImage(Swapchain* swapchain) { CreateImage(swapchain->extent.width, swapchain->extent.height); }
	void SubmitObjects(const VulkanCreationObject& creationObject, std::vector<Object*> objects);

	VkImage RTImage = VK_NULL_HANDLE;

	static int raySampleCount;
	static int rayDepth;
	static bool showNormals;
	static bool renderProgressive;

private:

	void UpdateMaterialDescriptorSets();
	void CreateMaterialBuffers();
	void CreateShaderBindingTable();
	void CreateImage(uint32_t width, uint32_t height);
	void UpdateDescriptorSets();

	std::vector<BottomLevelAccelerationStructure*> BLASs;
	TopLevelAccelerationStructure* TLAS = nullptr;

	Win32Window* window = nullptr;
	Swapchain* swapchain = nullptr;

	VkDevice logicalDevice;

	VkDeviceMemory RTImageMemory = VK_NULL_HANDLE;
	VkImageView RTImageView = VK_NULL_HANDLE;

	VkCommandPool commandPool = VK_NULL_HANDLE;
	PhysicalDevice physicalDevice = VK_NULL_HANDLE;

	VkBuffer materialBuffer = VK_NULL_HANDLE;
	VkDeviceMemory materialBufferMemory = VK_NULL_HANDLE;

	VkBuffer materialIndexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory materialIndexBufferMemory = VK_NULL_HANDLE;
};