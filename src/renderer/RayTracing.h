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

class RayTracing
{
public:
	RayTracing() {}
	void Destroy();
	void Init(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface,  Win32Window* window, Swapchain* swapchain);
	void DrawFrame(std::vector<Object*> objects, Win32Window* window, Camera* camera, VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void RecreateImage(Win32Window* window);
	void CopyImagesToDenoisingBuffers(VkCommandBuffer commandBuffer);
	void CopyDenoisedBufferToImage(VkCommandBuffer commandBuffer);
	void DenoiseImage(VkCommandBuffer commandBuffer);
	cudaStream_t GetCudaStream() { return cudaStream; }

	std::array<VkImage, 3> gBuffers{};
	void* handleBufferMemPointer = nullptr;

	static int  raySampleCount;
	static int  rayDepth;
	static bool showNormals;
	static bool showUniquePrimitives;
	static bool showAlbedo;
	static bool renderProgressive;
	static bool useWhiteAsAlbedo;
	
private:
	void InitOptix();
	void UpdateInstanceDataBuffer(const std::vector<Object*>& objects);
	void UpdateTextureBuffer();
	void UpdateMeshDataDescriptorSets();
	void CreateMeshDataBuffers();
	void CreateShaderBindingTable();
	void CreateImage(uint32_t width, uint32_t height);
	void UpdateDescriptorSets();
	void AllocateOptixBuffers(uint32_t width, uint32_t height);
	void DestroyOptixBuffers();
	void CreateExternalCudaBuffer(VkBuffer& buffer, VkDeviceMemory& memory, void** cuPtr, HANDLE& handle, VkDeviceSize size);
	void CreateExternalSemaphore(VkSemaphore& semaphore, HANDLE& handle, cudaExternalSemaphore_t& cuPtr);

	CUcontext cudaContext;
	CUdevice cudaDevice;

	CUdeviceptr stateBuffer = 0;
	CUdeviceptr scratchBuffer = 0;
	CUdeviceptr minRGB = 0;

	cudaStream_t cudaStream = {};
	
	OptixDeviceContext optixContext;
	OptixDenoiser denoiser;
	OptixDenoiserSizes m_denoiserSizes;

	size_t denoiserStateInBytes = 0;

	uint32_t amountOfActiveObjects = 0;
	uint32_t width = 0, height = 0;
	
	std::vector<BottomLevelAccelerationStructure*> BLASs;
	TopLevelAccelerationStructure* TLAS = nullptr;

	Win32Window* window  = nullptr;
	Swapchain* swapchain = nullptr;

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

	VkBuffer denoiseCopyBuffer = VK_NULL_HANDLE;
	VkDeviceMemory denoiseCopyMemory = VK_NULL_HANDLE;
	void* cuDenoisecopyBuffer = 0;
	HANDLE copyHandle = (void*)0;
	OptixImage2D inputImage{};

	VkBuffer normalDenoiseBuffer = VK_NULL_HANDLE;
	VkDeviceMemory normalDenoiseMemory = VK_NULL_HANDLE;
	void* cuNormalDenoise = 0;
	HANDLE normalHandle = (void*)0;
	OptixImage2D normalImage;

	VkBuffer albedoDenoiseBuffer = VK_NULL_HANDLE;
	VkDeviceMemory albedoDenoiseMemory = VK_NULL_HANDLE;
	void* cuAlbedoDenoise = 0;
	HANDLE albedoHandle = (void*)0;
	OptixImage2D albedoImage;

	OptixDenoiserSizes denoiserSizes{};

	VkBuffer outputBuffer = VK_NULL_HANDLE;
	VkDeviceMemory outputMemory = VK_NULL_HANDLE;
	void* cuOutputBuffer = 0;
	HANDLE outputHandle = (void*)0;
	OptixImage2D outputImage{};

	VkSemaphore semaphore;
	cudaExternalSemaphore_t cuSemaphore;
	HANDLE semaphoreHandle;

	std::array<VkDeviceMemory, 3> gBufferMemories;
	std::array<VkImageView, 3> gBufferViews;
	

	std::unordered_map<int, Handle> processedMaterials;
};