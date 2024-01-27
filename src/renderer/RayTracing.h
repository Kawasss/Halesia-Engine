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
class ShaderGroupReflector;

class RayTracing
{
public:
	RayTracing() {}
	void Destroy();
	
	static RayTracing* Create(Win32Window* window, Swapchain* swapchain);
	void DrawFrame(std::vector<Object*> objects, Win32Window* window, Camera* camera, uint32_t width, uint32_t height, VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void RecreateImage(Win32Window* window);
	void ApplyDenoisedImage(VkCommandBuffer commandBuffer);
	void PrepareForDenoising(VkCommandBuffer commandBuffer);
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
	void UpdateInstanceDataBuffer(const std::vector<Object*>& objects, Camera* camera);
	void UpdateTextureBuffer();
	void UpdateMeshDataDescriptorSets();
	void CreateShaderBindingTable();
	void CreateImage(uint32_t width, uint32_t height);
	void UpdateDescriptorSets();

	void Init(Win32Window* window, Swapchain* swapchain);
	void SetUp(Win32Window* window, Swapchain* swapchain);
	void CreateDescriptorPool();
	void CreateDescriptorSets(const std::vector<std::vector<char>> shaderCodes);
	void CreateRayTracingPipeline(const std::vector<std::vector<char>> shaderCodes);
	void CreateBuffers();

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

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout materialSetLayout;
	std::vector<VkDescriptorSet> descriptorSets;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR  };

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	VkBuffer uniformBufferBuffer;
	VkDeviceMemory uniformBufferMemory;

	std::array<VkDeviceMemory, 3> gBufferMemories;
	std::unordered_map<int, Handle> processedMaterials;
};