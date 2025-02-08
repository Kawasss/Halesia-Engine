#pragma once
#include <array>
#include <string>
#include <memory>

#include "RenderPipeline.h"
#include "FramesInFlight.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "Light.h"

class GraphicsPipeline;
class TopLevelAccelerationStructure;
class RayTracingPipeline;
class Skybox;

class DeferredPipeline : public RenderPipeline
{
public:
	~DeferredPipeline();

	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<Object*>& objects) override;

	void Resize(const Payload& payload) override;
	void AddLight(const Light& light) override;

	void LoadSkybox(const std::string& path);

private:
	static constexpr size_t GBUFFER_COUNT = 4;

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
	void CreateBuffers();
	void BindResources();
	void BindTLAS();

	void CreateRenderPass(const std::array<VkFormat, GBUFFER_COUNT>& formats);
	void CreatePipelines(VkRenderPass firstPass, VkRenderPass secondPass);

	void CreateAndBindRTGI();

	Skybox* CreateNewSkybox(const std::string& path);

	Framebuffer framebuffer;

	FIF::Buffer uboBuffer;
	FIF::Buffer lightBuffer;

	std::unique_ptr<GraphicsPipeline> firstPipeline;
	std::unique_ptr<GraphicsPipeline> secondPipeline;
	std::unique_ptr<RayTracingPipeline> rtgiPipeline;

	std::unique_ptr<TopLevelAccelerationStructure> TLAS;

	std::unique_ptr<Skybox> skybox;

	std::array<std::vector<uint64_t>, FIF::FRAME_COUNT> processedMats;
};