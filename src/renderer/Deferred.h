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
class ComputeShader;
class TopLevelAccelerationStructure;
class RayTracingPipeline;
class Skybox;

class DeferredPipeline : public RenderPipeline
{
public:
	DeferredPipeline() = default;
	~DeferredPipeline();

	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<MeshObject*>& objects) override;

	void Resize(const Payload& payload) override;

	void LoadSkybox(const std::string& path);

	void ReloadShaders(const Payload& payload) override;

	void OnRenderingBufferResize(const Payload& payload) override;

	std::vector<IntVariable> GetIntVariables() override;

private:
	static constexpr size_t GBUFFER_COUNT = 5;

	struct PushConstant;
	struct RTGIConstants;
	struct TAAConstants;
	struct SpatialConstants;
	struct InstanceData;
	struct SecondConstants;

	struct UBO
	{
		glm::vec4 camPos;
		glm::mat4 view;
		glm::mat4 proj;
		glm::mat4 prevView;
		glm::mat4 prevProj;
	};

	void UpdateUBO(Camera* cam);
	void CreateBuffers();
	void BindResources(const FIF::Buffer& lightBuffer);
	void BindTLAS();
	void BindGBuffers();

	void CreateAndPreparePipelines(const Payload& payload);
	void CreateRenderPass(const std::array<VkFormat, GBUFFER_COUNT>& formats);
	void CreatePipelines(VkRenderPass firstPass, VkRenderPass secondPass);

	void CreateTAAPipeline();
	void CreateTAAResources(uint32_t width, uint32_t height);
	void BindTAAResources();
	void ResizeTAA(uint32_t width, uint32_t height);
	void CopyResourcesForNextTAA(const CommandBuffer& cmdBuffer);
	void PushTAAConstants(const CommandBuffer& cmdBuffer, const Camera* camera);

	void TransitionResourcesToTAA(const CommandBuffer& cmdBuffer);
	void TransitionResourcesFromTAA(const CommandBuffer& cmdBuffer);

	void CreateRTGIPipeline(const Payload& payload);
	void CreateAndBindRTGI(const Payload& payload); // maybe seperate RTGI into its own class ??
	void PushRTGIConstants(const Payload& payload);
	void ResizeRTGI(uint32_t width, uint32_t height);
	void BindRTGIResources();
	void SetRTGIImageLayout();
	void SetInstanceData(const std::vector<MeshObject*>& objects);

	void CopyDenoisedToRTGI(const CommandBuffer& cmdBuffer);

	void PerformRayTracedRendering(const CommandBuffer& cmdBuffer, const Payload& payload);
	void PerformFirstDeferred(const CommandBuffer& cmdBuffer, const Payload& payload, const std::vector<MeshObject*>& objects);
	void PerformSecondDeferred(const CommandBuffer& cmdBuffer, const Payload& payload);
	
	void CopyDeferredDepthToResultDepth(const CommandBuffer& cmdBuffer, const Payload& payload);

	void RecreatePipelines(const Payload& payload);

	Skybox* CreateNewSkybox(const std::string& path);

	VkImageView GetPositionView() { return framebuffer.GetViews()[0]; }
	VkImageView GetAlbedoView()   { return framebuffer.GetViews()[1]; }
	VkImageView GetNormalView()   { return framebuffer.GetViews()[2]; }
	VkImageView GetMRAOView()     { return framebuffer.GetViews()[3]; }
	VkImageView GetVelocityView() { return framebuffer.GetViews()[4]; }

	uint32_t GetRTGIWidth()  const;
	uint32_t GetRTGIHeight() const;

	Framebuffer framebuffer;

	FIF::Buffer uboBuffer;
	FIF::Buffer instanceBuffer;

	vvm::SmartImage rtgiImage;
	VkImageView rtgiView = VK_NULL_HANDLE;

	vvm::SmartImage prevRtgiImage;
	VkImageView prevRtgiView = VK_NULL_HANDLE;

	vvm::SmartImage prevDepthImage;
	VkImageView prevDepthView = VK_NULL_HANDLE;

	vvm::SmartImage denoisedRtgiImage;
	VkImageView denoisedRtgiView = VK_NULL_HANDLE;

	vvm::SmartImage spatialDenoisedImage;
	VkImageView spatialDenoisedView = VK_NULL_HANDLE;

	std::unique_ptr<GraphicsPipeline> firstPipeline;
	std::unique_ptr<GraphicsPipeline> secondPipeline;
	std::unique_ptr<ComputeShader> taaPipeline;
	std::unique_ptr<ComputeShader> spatialPipeline;
	std::unique_ptr<RayTracingPipeline> rtgiPipeline;

	std::unique_ptr<TopLevelAccelerationStructure> TLAS;

	std::unique_ptr<Skybox> skybox;

	std::array<std::vector<uint64_t>, FIF::FRAME_COUNT> processedMats;

	glm::mat4 prevView;
	glm::mat4 prevProj;

	FIF::Buffer TAASampleBuffer;

	uint32_t frame = 0;

	int maxSampleCountTAA = 40;
	int rtgiSampleCount = 2;
	int rtgiBounceCount = 2;

	int spatialStepCount = 1;
};