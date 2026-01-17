module;

#include "RenderPipeline.h"
#include "Texture.h"

export module Renderer.SkyPipeline;

import std;

import Renderer.GraphicsPipeline;

export class SkyPipeline : public RenderPipeline
{
public:
	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<MeshObject*>& objects); // does not render to the screen, only calculates the LUTs
	void Render(const Payload& payload); // renders to the screen

	void ReloadShaders(const Payload& payload) override;
	void Resize(const Payload& payload) override;

	VkImageView GetTransmittanceView() const;
	VkImageView GetMScatteringView() const;
	VkImageView GetLatLongView() const;

private:
	void CreatePipelines();
	void CreateImages(const CommandBuffer& cmdBuffer, uint32_t width, uint32_t height);

	void bindImagesToPipelines();

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