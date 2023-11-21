#pragma once
#include <unordered_map>
#include "renderer/PhysicalDevice.h"
#include "../ResourceManager.h"

class Swapchain;
class BottomLevelAccelerationStructure;
class TopLevelAccelerationStructure;
class Win32Window;
class Object;
class Camera;

class RayTracing
{
public:
	RayTracing() {}
	void Destroy();
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface,  Win32Window* window, Swapchain* swapchain);
	void DrawFrame(std::vector<Object*> objects, Win32Window* window, Camera* camera, VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void RecreateImage(Swapchain* swapchain);

	VkImage RTImage = VK_NULL_HANDLE;

	static int  raySampleCount;
	static int  rayDepth;
	static bool showNormals;
	static bool showUniquePrimitives;
	static bool showAlbedo;
	static bool renderProgressive;
	
private:
	void UpdateModelMatrices(const std::vector<Object*>& objects);
	void UpdateInstanceDataBuffer(const std::vector<Object*>& objects);
	void UpdateTextureBuffer();
	void UpdateMeshDataDescriptorSets();
	void CreateMeshDataBuffers();
	void CreateShaderBindingTable();
	void CreateImage(uint32_t width, uint32_t height);
	void UpdateDescriptorSets();

	uint32_t amountOfActiveObjects = 0;

	std::vector<BottomLevelAccelerationStructure*> BLASs;
	TopLevelAccelerationStructure* TLAS = nullptr;

	Win32Window* window  = nullptr;
	Swapchain* swapchain = nullptr;

	bool imageHasChanged = false;

	VkDevice logicalDevice					= VK_NULL_HANDLE;

	VkDeviceMemory RTImageMemory			= VK_NULL_HANDLE;
	VkImageView RTImageView					= VK_NULL_HANDLE;

	VkCommandPool commandPool				= VK_NULL_HANDLE;
	PhysicalDevice physicalDevice			= VK_NULL_HANDLE;

	VkBuffer materialBuffer					= VK_NULL_HANDLE;
	VkDeviceMemory materialBufferMemory		= VK_NULL_HANDLE;

	void* modelMatrixMemoryPointer			= nullptr;
	VkBuffer modelMatrixBuffer				= VK_NULL_HANDLE;
	VkDeviceMemory modelMatrixBufferMemory	= VK_NULL_HANDLE;

	void* instanceMeshDataPointer			= nullptr;
	VkBuffer instanceMeshDataBuffer			= VK_NULL_HANDLE;
	VkDeviceMemory instanceMeshDataMemory	= VK_NULL_HANDLE;

	std::unordered_map<int, Handle> processedMaterials;
};