#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include "cuda_runtime_api.h"
#include "optix.h"

typedef void* HANDLE;

class Denoiser
{
public:
	static Denoiser* Create();
	void Destroy();

	void CopyImagesToDenoisingBuffers(VkCommandBuffer commandBuffer, std::array<VkImage, 4> gBuffers);
	void CopyDenoisedBufferToImage(VkCommandBuffer commandBuffer, VkImage image);
	void AllocateBuffers(uint32_t width, uint32_t height);
	void DenoiseImage();

	cudaStream_t GetCudaStream() { return cudaStream; }

private:
	struct SharedOptixImage
	{
		VkBuffer vkBuffer = VK_NULL_HANDLE;
		VkDeviceMemory vkMemory = VK_NULL_HANDLE;
		void* cudaBuffer = 0;
		HANDLE winHandle = (void*)0;
		OptixImage2D image{};

		void Create(uint32_t width, uint32_t height, Denoiser* denoiser); // the pointer is more of a hack
		void Destroy(VkDevice logicalDevice);
	};

	void CreateExternalCudaBuffer(VkBuffer& buffer, VkDeviceMemory& memory, void** cuPtr, HANDLE& handle, VkDeviceSize size);
	void CreateExternalSemaphore(VkSemaphore& semaphore, HANDLE& handle, cudaExternalSemaphore_t& cuPtr);
	
	void InitOptix();
	void DestroyBuffers();

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

	SharedOptixImage input{};
	SharedOptixImage normal{};
	SharedOptixImage albedo{};
	SharedOptixImage output{};
	SharedOptixImage motion{};
	SharedOptixImage previousFrame{};
};