#pragma once
#include <array>
#include <string>
#include <memory>

#include "RenderPipeline.h"
#include "FramesInFlight.h"
#include "VideoMemoryManager.h"
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
	DeferredPipeline() = default;
	~DeferredPipeline();

	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<Object*>& objects) override;

	void Resize(const Payload& payload) override;

	void LoadSkybox(const std::string& path);

private:
	static constexpr size_t GBUFFER_COUNT = 4;

	struct RTGIConstants
	{
		uint32_t frame;
	};

	struct UBO
	{
		glm::vec4 camPos;
		glm::mat4 view;
		glm::mat4 proj;
	};

	void UpdateTextureBuffer();
	void SetTextureBuffer();
	void UpdateUBO(Camera* cam);
	void CreateBuffers();
	void BindResources(VkBuffer lightBuffer);
	void BindTLAS();
	void BindGBuffers();

	void CreateRenderPass(const std::array<VkFormat, GBUFFER_COUNT>& formats);
	void CreatePipelines(VkRenderPass firstPass, VkRenderPass secondPass);

	void CreateAndBindRTGI(uint32_t width, uint32_t height); // maybe seperate RTGI into its own class ??
	void PushRTGIConstants(const Payload& payload);
	void ResizeRTGI(uint32_t width, uint32_t height);
	void BindRTGIResources();
	void SetRTGIImageLayout();

	void TransitionRTGIToRead(const CommandBuffer& cmdBuffer);
	void TransitionRTGIToWrite(const CommandBuffer& cmdBuffer);

	Skybox* CreateNewSkybox(const std::string& path);

	VkImageView GetPositionView() { return framebuffer.GetViews()[0]; }
	VkImageView GetAlbedoView()   { return framebuffer.GetViews()[1]; }
	VkImageView GetNormalView()   { return framebuffer.GetViews()[2]; }
	VkImageView GetMRAOView()     { return framebuffer.GetViews()[3]; }

	Framebuffer framebuffer;

	FIF::Buffer uboBuffer;

	vvm::SmartImage rtgiImage;
	VkImageView rtgiView = VK_NULL_HANDLE;

	std::unique_ptr<GraphicsPipeline> firstPipeline;
	std::unique_ptr<GraphicsPipeline> secondPipeline;
	std::unique_ptr<RayTracingPipeline> rtgiPipeline;

	std::unique_ptr<TopLevelAccelerationStructure> TLAS;

	std::unique_ptr<Skybox> skybox;

	std::array<std::vector<uint64_t>, FIF::FRAME_COUNT> processedMats;

	uint32_t frame = 0;
};