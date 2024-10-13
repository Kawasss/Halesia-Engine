#pragma once
#include "RenderPipeline.h"
#include "Buffer.h"

class Cubemap;
class GraphicsPipeline;

class SkyboxPipeline : public RenderPipeline
{
public:
	void SetSkyBox(Cubemap* skybox); // this will transfer ownership of the cubemap to this pipeline

	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<Object*>& objects) override; // the pipeline expects to be called in an already active render pass
	void Destroy() override;

private:
	void CreateRenderPass();
	void CreatePipeline();

	FIF::Buffer ubo;

	GraphicsPipeline* pipeline = nullptr;
	Cubemap* cubemap = nullptr;
};