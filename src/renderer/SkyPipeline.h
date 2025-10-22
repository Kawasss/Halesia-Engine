#pragma once
#include <memory>

#include "RenderPipeline.h"
#include "VideoMemoryManager.h"
#include "Texture.h"
#include "Buffer.h"

class GraphicsPipeline;

class SkyPipeline : public RenderPipeline
{
public:
	~SkyPipeline();

	void Start(const Payload& payload) override;
	void Update(const Payload& payload, const std::vector<MeshObject*>& objects);

	void ReloadShaders(const Payload& payload) override;
	void Resize(const Payload& payload) override;

private:
	void CreatePipelines();
	void CreateImages(uint32_t width, uint32_t height);
	void CreateDynamicFramebuffer(uint32_t width, uint32_t height);
	void CreateBuffers();
	void UpdateBuffers();

	void bindImagesToPipelines();
	void BindBufferToPipelines();

	VkFramebuffer framebuffer = VK_NULL_HANDLE;

	Image transmittanceLUT;
	Image mscatteringLUT;
	Image latlongMap;

	std::unique_ptr<GraphicsPipeline> transmittancePipeline;
	std::unique_ptr<GraphicsPipeline> mscatteringPipeline;
	std::unique_ptr<GraphicsPipeline> latlongPipeline;
	std::unique_ptr<GraphicsPipeline> atmospherePipeline;
};