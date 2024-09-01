#pragma once
#include <unordered_set>

#include "RenderPipeline.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "Light.h"

class GraphicsPipeline;

class DeferredPipeline : public RenderPipeline
{
public:
	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<Object*>& objects) override;
	void Destroy() override;

	void Resize(const Payload& payload) override;
	void AddLight(const Light& light) override;

private:
	struct UBO
	{
		glm::vec4 camPos;
		glm::mat4 view;
		glm::mat4 proj;
	};

	struct LightBuffer
	{
		int count;
		Light lights[1024];
	};

	void UpdateTextureBuffer();
	void SetTextureBuffer();
	void UpdateUBO(Camera* cam);

	Framebuffer framebuffer;

	Buffer uboBuffer;
	UBO* ubo;

	Buffer lightBuffer;
	LightBuffer* lights; // maybe shouldnt permanently map this big a buffer ??

	GraphicsPipeline* firstPipeline  = nullptr;
	GraphicsPipeline* secondPipeline = nullptr;

	std::vector<uint64_t> processedMats;
};