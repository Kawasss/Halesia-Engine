#include "renderer/SkyPipeline.h"
#include "renderer/GarbageManager.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Vulkan.h"

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
	vgm::Delete(transmittanceView);
	vgm::Delete(mscatteringView);
	vgm::Delete(latlongView);

	

	// actually create images here...
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

SkyPipeline::~SkyPipeline()
{
	vgm::Delete(transmittanceView);
	vgm::Delete(mscatteringView);
	vgm::Delete(latlongView);
	vgm::Delete(framebuffer);
}