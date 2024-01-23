#pragma once
#include <unordered_map>
#include "renderer/PhysicalDevice.h"
#include "../ResourceManager.h"
#include "optix.h"
#include <array>
#include "cuda_runtime_api.h"

typedef void* HANDLE;
class Swapchain;
class BottomLevelAccelerationStructure;
class TopLevelAccelerationStructure;
class Win32Window;
class Object;
class Camera;
class Denoiser;

class RayTracing
{
public:
	RayTracing() {}
	void Destroy();
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface,  Win32Window* window, Swapchain* swapchain);
	void DrawFrame(std::vector<Object*> objects, Win32Window* window, Camera* camera, uint32_t width, uint32_t height, VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void RecreateImage(Win32Window* window);
	void ApplyDenoisedImage(VkCommandBuffer commandBuffer);
	void DenoiseImage();

	std::array<VkImage, 3> gBuffers{};
	std::array<VkImageView, 3> gBufferViews;
	void* handleBufferMemPointer = nullptr;

	static int  raySampleCount;
	static int  rayDepth;
	static bool showNormals;
	static bool showUniquePrimitives;
	static bool showAlbedo;
	static bool renderProgressive;
	static bool useWhiteAsAlbedo;
	static glm::vec3 directionalLightDir;

	VkSemaphore externSemaphore;
	
private:
	void UpdateInstanceDataBuffer(const std::vector<Object*>& objects);
	void UpdateTextureBuffer();
	void UpdateMeshDataDescriptorSets();
	void CreateMeshDataBuffers();
	void CreateShaderBindingTable();
	void CreateImage(uint32_t width, uint32_t height);
	void UpdateDescriptorSets();

	uint32_t amountOfActiveObjects = 0;
	uint32_t width = 0, height = 0;
	
	std::vector<BottomLevelAccelerationStructure*> BLASs;
	TopLevelAccelerationStructure* TLAS = nullptr;

	Win32Window* window  = nullptr;
	Swapchain* swapchain = nullptr;
	Denoiser* denoiser = nullptr;

	bool imageHasChanged = false;

	VkDevice logicalDevice					= VK_NULL_HANDLE;

	VkCommandPool commandPool				= VK_NULL_HANDLE;
	PhysicalDevice physicalDevice			= VK_NULL_HANDLE;

	VkBuffer handleBuffer					= VK_NULL_HANDLE;
	VkDeviceMemory handleBufferMemory		= VK_NULL_HANDLE;

	VkBuffer materialBuffer					= VK_NULL_HANDLE;
	VkDeviceMemory materialBufferMemory		= VK_NULL_HANDLE;

	void* instanceMeshDataPointer			= nullptr;
	VkBuffer instanceMeshDataBuffer			= VK_NULL_HANDLE;
	VkDeviceMemory instanceMeshDataMemory	= VK_NULL_HANDLE;

	std::array<VkDeviceMemory, 3> gBufferMemories;

	std::unordered_map<int, Handle> processedMaterials;
};