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
	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<MeshObject*>& objects);

	void ReloadShaders(const Payload& payload) override;
	void Resize(const Payload& payload) override;

private:
	void CreatePipelines();
	void CreateImages(const CommandBuffer& cmdBuffer, uint32_t width, uint32_t height);
	void CreateBuffers();
	void UpdateBuffers();

	void bindImagesToPipelines();
	void BindBufferToPipelines();

	void BeginRenderPass(const CommandBuffer& cmdBuffer, VkImageView view, uint32_t width, uint32_t height);
	void BeginRenderPass(const CommandBuffer& cmdBuffer, Image& image);
	void EndRenderPass(const CommandBuffer& cmdBuffer, Image& image);

	void BeginPresentationRenderPass(const CommandBuffer& cmdBuffer, Renderer* renderer, uint32_t width, uint32_t height);

	Image transmittanceLUT;
	Image mscatteringLUT;
	Image latlongMap;

	std::unique_ptr<GraphicsPipeline> transmittancePipeline;
	std::unique_ptr<GraphicsPipeline> mscatteringPipeline;
	std::unique_ptr<GraphicsPipeline> latlongPipeline;
	std::unique_ptr<GraphicsPipeline> atmospherePipeline;
};