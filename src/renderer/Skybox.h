#pragma once
#include "Buffer.h"

class Cubemap;
class GraphicsPipeline;
class Texture;
class CommandBuffer;
class Camera;
class Renderer;

class Skybox
{
public:
	void SetSkyBox(Cubemap* skybox); // this will transfer ownership of the cubemap to this pipeline

	void Start();
	void Draw(const CommandBuffer& cmdBuffer, Camera* camera, Renderer* renderer); // the pipeline expects to be called in an already active render pass
	void Destroy();

private:
	void ConvertImageToCubemap(const CommandBuffer& cmdBuffer);
	void SetupConvert();

	void CreateRenderPass();
	void CreatePipeline();

	FIF::Buffer ubo;

	GraphicsPipeline* convertPipeline = nullptr;
	GraphicsPipeline* pipeline = nullptr;
	Cubemap* cubemap = nullptr;
	Texture* texture = nullptr;

	// these are stored here rn because i dont have good place to destroy them yet
	VkRenderPass renderPass;
	VkRenderPass convertRenderPass;
	VkFramebuffer framebuffer;
	bool hasConverted = false;
};