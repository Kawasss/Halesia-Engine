#pragma once
#include <array>

#include "RenderPipeline.h"
#include "FramesInFlight.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "Light.h"

class GraphicsPipeline;
class TopLevelAccelerationStructure;

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

	void CreateRenderPass(const std::vector<VkFormat>& formats);
	void CreatePipelines(VkRenderPass firstPass, VkRenderPass secondPass);

	Framebuffer framebuffer;

	FIF::Buffer uboBuffer;
	FIF::Buffer lightBuffer;

	GraphicsPipeline* firstPipeline  = nullptr;
	GraphicsPipeline* secondPipeline = nullptr;

	TopLevelAccelerationStructure* TLAS = nullptr;

	std::array<std::vector<uint64_t>, FIF::FRAME_COUNT> processedMats;
};