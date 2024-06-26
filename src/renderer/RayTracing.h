#pragma once
#include <unordered_map>
#include <array>

#include "PhysicalDevice.h"
#include "Buffer.h"

#include "../ResourceManager.h"

#include "optix.h"
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
struct InstanceMeshData;

class RayTracing
{
public:
	RayTracing() {}
	void Destroy();
	
	static RayTracing* Create(Win32Window* window, Swapchain* swapchain);
	void DrawFrame(std::vector<Object*> objects, Win32Window* window, Camera* camera, uint32_t width, uint32_t height, VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void RecreateImage(uint32_t width, uint32_t height);
	void ApplyDenoisedImage(VkCommandBuffer commandBuffer);
	void PrepareForDenoising(VkCommandBuffer commandBuffer);
	void DenoiseImage();

	std::array<VkImage, 3> gBuffers{};
	std::array<VkImageView, 3> gBufferViews;
	uint64_t* handleBufferMemPointer = nullptr;

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
	void CreateDescriptorPool(const ShaderGroupReflector& groupReflection);
	void CreateDescriptorSets(const ShaderGroupReflector& groupReflection);
	void CreateRayTracingPipeline(const std::vector<std::vector<char>> shaderCodes);
	void CreateBuffers(const ShaderGroupReflector& groupReflection);
	void CopyPreviousResult(VkCommandBuffer commandBuffer);
	void CreateMotionBuffer();
		
	uint32_t amountOfActiveObjects = 0;
	uint32_t width = 0, height = 0;
	
	std::vector<BottomLevelAccelerationStructure*> BLASs;
	TopLevelAccelerationStructure* TLAS = nullptr;

	Win32Window* window  = nullptr;
	Swapchain* swapchain = nullptr;
	Denoiser* denoiser = nullptr;

	bool imageHasChanged = false;

	VkDevice logicalDevice		  = VK_NULL_HANDLE;
	VkCommandPool commandPool	  = VK_NULL_HANDLE;

	Buffer handleBuffer;
	Buffer materialBuffer;
	Buffer uniformBufferBuffer;
	Buffer motionBuffer;
	Buffer instanceMeshDataBuffer;
	InstanceMeshData* instanceMeshDataPointer = nullptr;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSetLayout materialSetLayout;
	std::vector<VkDescriptorSet> descriptorSets;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR  };

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	VkImage prevImage;
	VkImageView prevImageView;
	VkDeviceMemory prevMemory;

	std::array<VkDeviceMemory, 4> gBufferMemories;
	std::unordered_map<int, Handle> processedMaterials;
};