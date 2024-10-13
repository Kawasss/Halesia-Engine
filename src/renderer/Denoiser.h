#pragma once
#include <vulkan/vulkan.h>
#include <array>

typedef void* HANDLE;

class Denoiser
{
public:
	static Denoiser* Create();
	~Denoiser() { Destroy(); }
	void Destroy();

	void CopyImagesToDenoisingBuffers(VkCommandBuffer commandBuffer, std::array<VkImage, 3> gBuffers);
	void CopyDenoisedBufferToImage(VkCommandBuffer commandBuffer, VkImage image);
	void AllocateBuffers(uint32_t width, uint32_t height);
	void DenoiseImage();

private:
};