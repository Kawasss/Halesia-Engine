#pragma once
#include "RenderPipeline.h"
#include "Buffer.h"

class Cubemap;
class GraphicsPipeline;
class Texture;
struct Mesh;

class SkyboxPipeline : public RenderPipeline
{
public:
	void SetSkyBox(Cubemap* skybox); // this will transfer ownership of the cubemap to this pipeline

	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<Object*>& objects) override; // the pipeline expects to be called in an already active render pass
	void Destroy() override;

private:
	void ConvertImageToCubemap(const Payload& payload);
	void SetupConvert(const Payload& payload);

	void CreateRenderPass();
	void CreatePipeline();

	FIF::Buffer ubo;

	GraphicsPipeline* convertPipeline = nullptr;
	GraphicsPipeline* pipeline = nullptr;
	Cubemap* cubemap = nullptr;
	Texture* texture = nullptr;

	// these are stored here rn because i dont have good place to destroy them yet
	VkRenderPass convertRenderPass;
	VkFramebuffer framebuffer;
	bool hasConverted = false;

	Mesh* cube;
};