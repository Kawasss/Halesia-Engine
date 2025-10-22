#include "renderer/SkyPipeline.h"
#include "renderer/GarbageManager.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Vulkan.h"

constexpr VkFormat LUT_FORMAT = VK_FORMAT_R8G8B8A8_UNORM;
constexpr uint32_t LUT_FORMAT_SIZE = sizeof(uint32_t);
constexpr VkImageUsageFlags LUT_USAGE = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

void SkyPipeline::Start(const Payload& payload)
{
	CreateBuffers();

	ReloadShaders(payload);
	Resize(payload);
}

void SkyPipeline::Update(const Payload& payload, const std::vector<MeshObject*>& objects)
{
	UpdateBuffers();
}

void SkyPipeline::ReloadShaders(const Payload& payload)
{
	CreatePipelines();
}

void SkyPipeline::Resize(const Payload& payload)
{
	CreateImages(payload.width, payload.height);
	CreateDynamicFramebuffer(payload.width, payload.height);
}

void SkyPipeline::CreateImages(uint32_t width, uint32_t height)
{
	transmittanceLUT.Destroy();
	mscatteringLUT.Destroy();
	latlongMap.Destroy();

	transmittanceLUT.Create(width, height, 1, LUT_FORMAT, LUT_FORMAT_SIZE, LUT_USAGE, Image::None);
	mscatteringLUT.Create(width, height, 1, LUT_FORMAT, LUT_FORMAT_SIZE, LUT_USAGE, Image::None);
	latlongMap.Create(width, height, 1, LUT_FORMAT, LUT_FORMAT_SIZE, LUT_USAGE, Image::None);
}

void SkyPipeline::CreateDynamicFramebuffer(uint32_t width, uint32_t height)
{

}

void SkyPipeline::CreatePipelines()
{

}

void SkyPipeline::CreateBuffers()
{

}

void SkyPipeline::UpdateBuffers()
{

}

void SkyPipeline::bindImagesToPipelines()
{

}

void SkyPipeline::BindBufferToPipelines()
{

}

SkyPipeline::~SkyPipeline()
{
	vgm::Delete(framebuffer);
}