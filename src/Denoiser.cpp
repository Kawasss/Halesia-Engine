#include "renderer/Denoiser.h"

void Denoiser::AllocateBuffers(uint32_t width, uint32_t height)
{

}

void Denoiser::DenoiseImage()
{

}

void Denoiser::CopyImagesToDenoisingBuffers(VkCommandBuffer commandBuffer, std::array<VkImage, 3> gBuffers)
{

}

void Denoiser::CopyDenoisedBufferToImage(VkCommandBuffer commandBuffer, VkImage image)
{

}

void Denoiser::Destroy()
{

}

Denoiser* Denoiser::Create()
{
	Denoiser* ret = new Denoiser();
	return ret;
}