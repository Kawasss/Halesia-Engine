#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include "cuda_runtime_api.h"
#include "optix.h"

typedef void* HANDLE;
struct VulkanCreationObject;

class Denoiser
{
public:
	static Denoiser* Create(const VulkanCreationObject& creationObject);
	void Destroy();

	void CopyImagesToDenoisingBuffers(VkCommandBuffer commandBuffer, std::array<VkImage, 3> gBuffers);
	void CopyDenoisedBufferToImage(VkCommandBuffer commandBuffer, VkImage image);
	void AllocateBuffers(uint32_t width, uint32_t height);
	void DenoiseImage();

	cudaStream_t GetCudaStream() { return cudaStream; }

private:
	void CreateExternalCudaBuffer(VkBuffer& buffer, VkDeviceMemory& memory, void** cuPtr, HANDLE& handle, VkDeviceSize size);
	void CreateExternalSemaphore(VkSemaphore& semaphore, HANDLE& handle, cudaExternalSemaphore_t& cuPtr);
	
	void InitOptix();
	void DestroyBuffers();

	VkDevice logicalDevice;
	PhysicalDevice physicalDevice;

	VkSemaphore externSemaphore;

	cudaStream_t cudaStream = {};

	HANDLE externSemaphoreHandle;
	cudaExternalSemaphore_t cuExternSemaphore;

	CUcontext cudaContext;
	CUdevice cudaDevice;

	CUdeviceptr stateBuffer = 0;
	CUdeviceptr scratchBuffer = 0;
	CUdeviceptr minRGB = 0;

	OptixDeviceContext optixContext;
	OptixDenoiser denoiser;
	OptixDenoiserSizes denoiserSizes;

	size_t denoiserStateInBytes = 0;
	uint32_t width = 0, height = 0;

	VkBuffer copyBuffer = VK_NULL_HANDLE;
	VkDeviceMemory copyMemory = VK_NULL_HANDLE;
	void* cuCopyBuffer = 0;
	HANDLE copyHandle = (void*)0;
	OptixImage2D inputImage{};

	VkBuffer normalBuffer = VK_NULL_HANDLE;
	VkDeviceMemory normalMemory = VK_NULL_HANDLE;
	void* cuNormal = 0;
	HANDLE normalHandle = (void*)0;
	OptixImage2D normalImage;

	VkBuffer albedoBuffer = VK_NULL_HANDLE;
	VkDeviceMemory albedoMemory = VK_NULL_HANDLE;
	void* cuAlbedo = 0;
	HANDLE albedoHandle = (void*)0;
	OptixImage2D albedoImage;

	VkBuffer outputBuffer = VK_NULL_HANDLE;
	VkDeviceMemory outputMemory = VK_NULL_HANDLE;
	void* cuOutputBuffer = 0;
	HANDLE outputHandle = (void*)0;
	OptixImage2D outputImage{};
};