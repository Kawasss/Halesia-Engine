#include "renderer/Deferred.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/ComputeShader.h"
#include "renderer/Renderer.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/RayTracingPipeline.h"
#include "renderer/accelerationStructures.h"
#include "renderer/PipelineCreator.h"
#include "renderer/GarbageManager.h"
#include "renderer/ImageTransitioner.h"
#include "renderer/Texture.h"
#include "renderer/Skybox.h"
#include "renderer/Vulkan.h"

#include "core/MeshObject.h"
#include "core/Camera.h"

struct DeferredPipeline::PushConstant
{
	glm::mat4 model;
	glm::vec2 velocity;
	int materialID;
};

struct DeferredPipeline::RTGIConstants
{
	uint32_t frame;
	int32_t sampleCount;
	int32_t bounceCount;
	glm::vec3 position;
};

struct DeferredPipeline::TAAConstants
{
	glm::mat4 projInv;
	glm::mat4 projViewInv;
	glm::mat4 prevProjView;
	int width;
	int height;
	int maxSampleCount;
};

struct DeferredPipeline::SpatialConstants
{
	int width;
	int height;
	int stepCount;
};

struct DeferredPipeline::InstanceData
{
	InstanceData() = default;
	InstanceData(uint32_t v, uint32_t i, uint32_t m) : vertexOffset(v), indexOffset(i), material(m) {}

	uint32_t vertexOffset;
	uint32_t indexOffset;
	int material;
};

constexpr VkFormat GBUFFER_POSITION_FORMAT = VK_FORMAT_R32G32B32A32_SFLOAT; // 32 bit format takes a lot of performance compared to an 8 bit format
constexpr VkFormat GBUFFER_ALBEDO_FORMAT   = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_NORMAL_FORMAT   = VK_FORMAT_R16G16B16A16_SFLOAT; // 16 bit instead of 8 bit to allow for negative normals
constexpr VkFormat GBUFFER_MRAO_FORMAT     = VK_FORMAT_R8G8B8A8_UNORM;      // Metallic, Roughness and Ambient Occlusion
constexpr VkFormat GBUFFER_RTGI_FORMAT     = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_VELOCITY_FORMAT = VK_FORMAT_R16G16_SFLOAT;

constexpr TopLevelAccelerationStructure::InstanceIndexType RTGI_TLAS_INDEX_TYPE = TopLevelAccelerationStructure::InstanceIndexType::Identifier;

constexpr uint32_t RTGI_RESOLUTION_UPSCALE = 1;

void DeferredPipeline::Start(const Payload& payload)
{
	std::array<VkFormat, GBUFFER_COUNT> formats = 
	{ 
		GBUFFER_POSITION_FORMAT,
		GBUFFER_ALBEDO_FORMAT, 
		GBUFFER_NORMAL_FORMAT, 
		GBUFFER_MRAO_FORMAT,
		GBUFFER_VELOCITY_FORMAT,
	};

	CreateBuffers();
	CreateRenderPass(formats);

	for (int i = 0; i < formats.size() + 1; i++)
		framebuffer.SetImageUsage(i, VK_IMAGE_USAGE_SAMPLED_BIT);

	framebuffer.SetImageUsage(formats.size(), VK_IMAGE_USAGE_TRANSFER_SRC_BIT); // the depth buffer is to be transferred for TAA

	framebuffer.Init(renderPass, payload.width, payload.height, formats, 1.0f);

	if (Renderer::canRayTrace)
		TLAS.reset(TopLevelAccelerationStructure::Create());

	CreateAndPreparePipelines(payload);
}

void DeferredPipeline::CreateAndPreparePipelines(const Payload& payload)
{
	std::array<VkFormat, GBUFFER_COUNT> formats =
	{
		GBUFFER_POSITION_FORMAT,
		GBUFFER_ALBEDO_FORMAT,
		GBUFFER_NORMAL_FORMAT,
		GBUFFER_MRAO_FORMAT,
		GBUFFER_VELOCITY_FORMAT,
	};

	CreatePipelines(renderPass, payload.renderer->GetDefault3DRenderPass());

	BindResources(payload.renderer->GetLightBuffer());

	if (Renderer::canRayTrace)
	{
		CreateAndBindRTGI(payload);

		BindTLAS();
	}

	CreateTAAPipeline();
	CreateTAAResources(payload.width, payload.height);
	BindTAAResources();

	SetTextureBuffer();
}

void DeferredPipeline::ReloadShaders(const Payload& payload)
{
	firstPipeline.reset();
	secondPipeline.reset();
	rtgiPipeline.reset();

	Renderer::CompileShaderToSpirv("shaders/uncompiled/deferredFirst.vert");
	Renderer::CompileShaderToSpirv("shaders/uncompiled/deferredFirst.frag");
	Renderer::CompileShaderToSpirv("shaders/uncompiled/deferredSecond.vert");
	Renderer::CompileShaderToSpirv("shaders/uncompiled/deferredSecond.frag");
	Renderer::CompileShaderToSpirv("shaders/uncompiled/deferredSecondRT.frag");

	Renderer::CompileShaderToSpirv("shaders/uncompiled/rtgi.rgen");
	Renderer::CompileShaderToSpirv("shaders/uncompiled/rtgi.rchit");
	Renderer::CompileShaderToSpirv("shaders/uncompiled/rtgi.rmiss");

	Renderer::CompileShaderToSpirv("shaders/uncompiled/taa.comp");
	Renderer::CompileShaderToSpirv("shaders/uncompiled/spatial.comp");
	
	CreateAndPreparePipelines(payload);

	if (skybox != nullptr)
		rtgiPipeline->BindImageToName("skybox", skybox->GetCubemap()->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void DeferredPipeline::CreateAndBindRTGI(const Payload& payload)
{
	constexpr VkDeviceSize MAX_INSTANCE_COUNT = 500ULL;

	rtgiPipeline = std::make_unique<RayTracingPipeline>("shaders/spirv/rtgi.rgen.spv", "shaders/spirv/rtgi.rchit.spv", "shaders/spirv/rtgi.rmiss.spv");

	instanceBuffer.Init(sizeof(InstanceData) * MAX_INSTANCE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	instanceBuffer.MapPermanently();

	rtgiPipeline->BindBufferToName("instanceBuffer", instanceBuffer);
	rtgiPipeline->BindBufferToName("lights", payload.renderer->GetLightBuffer().Get());
	rtgiPipeline->BindBufferToName("vertexBuffer", Renderer::g_vertexBuffer.GetBufferHandle());
	rtgiPipeline->BindBufferToName("indexBuffer", Renderer::g_indexBuffer.GetBufferHandle());

	if (skybox != nullptr)
		rtgiPipeline->BindImageToName("skybox", skybox->GetCubemap()->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	ResizeRTGI(payload.width, payload.height);
}

void DeferredPipeline::PushRTGIConstants(const Payload& payload)
{
	RTGIConstants constants{};
	constants.frame = frame++;
	constants.sampleCount = rtgiSampleCount;
	constants.bounceCount = rtgiBounceCount;
	constants.position = payload.camera->position;

	rtgiPipeline->PushConstant(payload.commandBuffer, constants, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
}

void DeferredPipeline::ResizeRTGI(uint32_t width, uint32_t height)
{
	width  /= RTGI_RESOLUTION_UPSCALE;
	height /= RTGI_RESOLUTION_UPSCALE;

	if (rtgiImage.IsValid())
		rtgiImage.Destroy();

	if (rtgiView != VK_NULL_HANDLE)
		vgm::Delete(rtgiView);

	rtgiImage = Vulkan::CreateImage(width, height, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
	rtgiView = Vulkan::CreateImageView(rtgiImage.Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	SetRTGIImageLayout();
	BindRTGIResources();
}

void DeferredPipeline::SetRTGIImageLayout()
{
	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = rtgiImage.Get();
	memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memoryBarrier.subresourceRange.baseMipLevel = 0;
	memoryBarrier.subresourceRange.levelCount = 1;
	memoryBarrier.subresourceRange.baseArrayLayer = 0;
	memoryBarrier.subresourceRange.layerCount = 1;

	constexpr VkPipelineStageFlags src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	constexpr VkPipelineStageFlags dst = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

	Vulkan::ExecuteSingleTimeCommands([&](const CommandBuffer& cmdBuffer)
		{
			cmdBuffer.PipelineBarrier(src, dst, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
		}
	);
}

void DeferredPipeline::BindRTGIResources()
{
	rtgiPipeline->BindImageToName("globalIlluminationImage", rtgiView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_GENERAL);
	rtgiPipeline->BindImageToName("albedoImage", GetAlbedoView(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	rtgiPipeline->BindImageToName("normalImage", GetNormalView(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	rtgiPipeline->BindImageToName("positionImage", GetPositionView(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	secondPipeline->BindImageToName("globalIlluminationImage", rtgiView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_GENERAL);
}

void DeferredPipeline::CreateBuffers()
{
	uboBuffer.Init(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	uboBuffer.MapPermanently();
}

void DeferredPipeline::BindTLAS()
{
	VkWriteDescriptorSetAccelerationStructureKHR ASDescriptorInfo{};
	ASDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	ASDescriptorInfo.accelerationStructureCount = 1;
	ASDescriptorInfo.pAccelerationStructures = &TLAS->accelerationStructure;

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writeSet.pNext = &ASDescriptorInfo;
	writeSet.descriptorCount = 1;
	writeSet.dstBinding = 4;

	for (const std::vector<VkDescriptorSet>& sets : secondPipeline->GetAllDescriptorSets())
	{
		writeSet.dstSet = sets[0];
		vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, 1, &writeSet, 0, nullptr); // better to incorporate this into Pipeline
	}

	// also write the TLAS to the RTGI pipeline
	writeSet.dstBinding = 0;
	for (const std::vector<VkDescriptorSet>& sets : rtgiPipeline->GetAllDescriptorSets())
	{
		writeSet.dstSet = sets[0];
		vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, 1, &writeSet, 0, nullptr); // better to incorporate this into Pipeline
	}
}

void DeferredPipeline::BindResources(const FIF::Buffer& lightBuffer)
{
	firstPipeline->BindBufferToName("ubo", uboBuffer.Get());
	secondPipeline->BindBufferToName("lights", lightBuffer.Get());

	BindGBuffers();
}

void DeferredPipeline::BindGBuffers()
{
	constexpr VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	const VkSampler& sampler = Renderer::defaultSampler;
	const std::vector<VkImageView>& views = framebuffer.GetViews();

	secondPipeline->BindImageToName("positionImage", GetPositionView(), sampler, layout);
	secondPipeline->BindImageToName("albedoImage", GetAlbedoView(), sampler, layout);
	secondPipeline->BindImageToName("normalImage", GetNormalView(), sampler, layout);
	secondPipeline->BindImageToName("metallicRoughnessAOImage", GetMRAOView(), sampler, layout);
}

void DeferredPipeline::CreateRenderPass(const std::array<VkFormat, GBUFFER_COUNT>& formats)
{
	RenderPassBuilder builder(formats);

	builder.SetInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	builder.SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	builder.ClearOnLoad(true);

	renderPass = builder.Build();
}

void DeferredPipeline::CreatePipelines(VkRenderPass firstPass, VkRenderPass secondPass)
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/spirv/deferredFirst.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/deferredFirst.frag.spv";
	createInfo.renderPass = firstPass;
	
	firstPipeline = std::make_unique<GraphicsPipeline>(createInfo);

	createInfo.vertexShader   = "shaders/spirv/deferredSecond.vert.spv";
	createInfo.fragmentShader = Renderer::canRayTrace ? "shaders/spirv/deferredSecondRT.frag.spv" : "shaders/spirv/deferredSecond.frag.spv";
	createInfo.renderPass = secondPass;
	createInfo.noVertices = true;

	secondPipeline = std::make_unique<GraphicsPipeline>(createInfo);
}

void DeferredPipeline::CreateTAAPipeline()
{
	taaPipeline = std::make_unique<ComputeShader>("shaders/spirv/taa.comp.spv");
	spatialPipeline = std::make_unique<ComputeShader>("shaders/spirv/spatial.comp.spv");
}

void DeferredPipeline::CreateTAAResources(uint32_t width, uint32_t height)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	constexpr VkImageUsageFlags prevUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	const VkFormat depthFormat = ctx.physicalDevice.GetDepthFormat();

	TAASampleBuffer.Init(width * height * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	prevDepthImage = Vulkan::CreateImage(width, height, 1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, prevUsageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
	prevDepthView  = Vulkan::CreateImageView(prevDepthImage.Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	prevRtgiImage = Vulkan::CreateImage(width, height, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_TILING_OPTIMAL, prevUsageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
	prevRtgiView  = Vulkan::CreateImageView(prevRtgiImage.Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	denoisedRtgiImage = Vulkan::CreateImage(width, height, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
	denoisedRtgiView = Vulkan::CreateImageView(denoisedRtgiImage.Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	spatialDenoisedImage = Vulkan::CreateImage(width, height, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
	spatialDenoisedView = Vulkan::CreateImageView(spatialDenoisedImage.Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	Vulkan::ExecuteSingleTimeCommands([&](const CommandBuffer& cmdBuffer)
		{
			TAASampleBuffer.Fill(cmdBuffer);

			ImageTransitioner transitioner(prevDepthImage.Get());
			transitioner.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			transitioner.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			transitioner.srcAccess = 0;
			transitioner.dstAccess = VK_ACCESS_SHADER_READ_BIT;
			transitioner.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
			transitioner.width = width;
			transitioner.height = height;

			transitioner.Transition(cmdBuffer);

			transitioner.image = prevRtgiImage.Get();
			transitioner.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			transitioner.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

			transitioner.Transition(cmdBuffer);

			transitioner.image = denoisedRtgiImage.Get();

			transitioner.Transition(cmdBuffer);

			transitioner.image = spatialDenoisedImage.Get();

			transitioner.Transition(cmdBuffer);
		}
	);
}

void DeferredPipeline::BindTAAResources()
{
	taaPipeline->BindImageToName("depthImage", framebuffer.GetDepthView(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	taaPipeline->BindImageToName("prevDepthImage", prevDepthView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	taaPipeline->BindImageToName("baseImage", rtgiView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_GENERAL);
	taaPipeline->BindImageToName("prevBaseImage", prevRtgiView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_GENERAL);
	taaPipeline->BindImageToName("velocityImage", GetVelocityView(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	taaPipeline->BindImageToName("resultImage", denoisedRtgiView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_GENERAL);
	
	taaPipeline->BindBufferToName("sampleCountBuffer", TAASampleBuffer);

	spatialPipeline->BindImageToName("depthImage", framebuffer.GetDepthView(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	spatialPipeline->BindImageToName("inputImage", denoisedRtgiView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_GENERAL); 
	spatialPipeline->BindImageToName("normalImage", GetNormalView(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	spatialPipeline->BindImageToName("outputImage", spatialDenoisedView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_GENERAL);
}

void DeferredPipeline::ResizeTAA(uint32_t width, uint32_t height)
{
	if (prevDepthImage.IsValid())
		prevDepthImage.Destroy();

	if (prevDepthView != VK_NULL_HANDLE)
		vgm::Delete(prevDepthView);

	if (prevRtgiImage.IsValid())
		prevRtgiImage.Destroy();

	if (prevRtgiView != VK_NULL_HANDLE)
		vgm::Delete(prevRtgiView);

	if (denoisedRtgiImage.IsValid())
		denoisedRtgiImage.Destroy();

	if (denoisedRtgiView != VK_NULL_HANDLE)
		vgm::Delete(denoisedRtgiView);

	if (TAASampleBuffer.IsValid())
		TAASampleBuffer.Destroy();

	if (spatialDenoisedImage.IsValid())
		spatialDenoisedImage.Destroy();

	if (spatialDenoisedView != VK_NULL_HANDLE)
		vgm::Delete(spatialDenoisedView);

	CreateTAAResources(width, height);
	BindTAAResources();
}

void DeferredPipeline::CopyResourcesForNextTAA(const CommandBuffer& cmdBuffer)
{
	ImageTransitioner prevDepthTransition(prevDepthImage.Get());
	prevDepthTransition.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	prevDepthTransition.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	prevDepthTransition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	prevDepthTransition.srcAccess = VK_ACCESS_MEMORY_READ_BIT;
	prevDepthTransition.dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
	prevDepthTransition.srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	prevDepthTransition.dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	prevDepthTransition.width = framebuffer.GetWidth();
	prevDepthTransition.height = framebuffer.GetHeight();

	VkImage currentDepth = framebuffer.GetImages().back().Get();
	ImageTransitioner depthTransition(currentDepth);
	depthTransition.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthTransition.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // already transitioned from attachment to this by the TAA
	depthTransition.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	depthTransition.srcAccess = VK_ACCESS_MEMORY_READ_BIT;
	depthTransition.dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
	depthTransition.srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	depthTransition.dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
	depthTransition.width = framebuffer.GetWidth();
	depthTransition.height = framebuffer.GetHeight();

	prevDepthTransition.Transition(cmdBuffer);
	depthTransition.Transition(cmdBuffer);

	VkImageCopy copy{};
	copy.extent = { framebuffer.GetWidth(), framebuffer.GetHeight(), 1 };
	copy.srcOffset = { 0, 0, 0 };
	copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	copy.srcSubresource.baseArrayLayer = 0;
	copy.srcSubresource.layerCount = 1;
	copy.srcSubresource.mipLevel = 0;
	copy.dstOffset = { 0, 0, 0 };
	copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	copy.dstSubresource.baseArrayLayer = 0;
	copy.dstSubresource.layerCount = 1;
	copy.dstSubresource.mipLevel = 0;

	VkImageCopy rtgiCopy{};
	rtgiCopy.extent = { GetRTGIWidth(), GetRTGIHeight(), 1};
	rtgiCopy.srcOffset = { 0, 0, 0 };
	rtgiCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	rtgiCopy.srcSubresource.baseArrayLayer = 0;
	rtgiCopy.srcSubresource.layerCount = 1;
	rtgiCopy.srcSubresource.mipLevel = 0;
	rtgiCopy.dstOffset = { 0, 0, 0 };
	rtgiCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	rtgiCopy.dstSubresource.baseArrayLayer = 0;
	rtgiCopy.dstSubresource.layerCount = 1;
	rtgiCopy.dstSubresource.mipLevel = 0;

	cmdBuffer.CopyImage(framebuffer.GetDepthImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, prevDepthImage.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	cmdBuffer.CopyImage(spatialDenoisedImage.Get(), VK_IMAGE_LAYOUT_GENERAL, prevRtgiImage.Get(), VK_IMAGE_LAYOUT_GENERAL, 1, &rtgiCopy);

	depthTransition.Detransition(cmdBuffer);
	prevDepthTransition.Detransition(cmdBuffer);
}

void DeferredPipeline::TransitionResourcesFromTAA(const CommandBuffer& cmdBuffer)
{
	/*VkImage currentDepth = framebuffer.GetImages().back().Get();
	ImageTransitioner depthTransition(currentDepth);
	depthTransition.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthTransition.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthTransition.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthTransition.srcAccess = VK_ACCESS_MEMORY_READ_BIT;
	depthTransition.dstAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	depthTransition.srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	depthTransition.dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	depthTransition.width = framebuffer.GetWidth();
	depthTransition.height = framebuffer.GetHeight();

	depthTransition.Transition(cmdBuffer);*/
}

void DeferredPipeline::TransitionResourcesToTAA(const CommandBuffer& cmdBuffer)
{
	// images that are already transitioned:
	// - previous depth
	// - velocity
	// - RTGI (never transition: VK_IMAGE_LAYOUT_GENERAL)

	VkImage currentDepth = framebuffer.GetImages().back().Get();
	ImageTransitioner depthTransition(currentDepth);
	depthTransition.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthTransition.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthTransition.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	depthTransition.srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	depthTransition.dstAccess = VK_ACCESS_MEMORY_READ_BIT;
	depthTransition.srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	depthTransition.dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	depthTransition.width = framebuffer.GetWidth();
	depthTransition.height = framebuffer.GetHeight();

	depthTransition.Transition(cmdBuffer);
}

void DeferredPipeline::CopyDenoisedToRTGI(const CommandBuffer& cmdBuffer)
{
	VkImageCopy copy{};
	copy.dstOffset = { 0, 0, 0 };
	copy.srcOffset = { 0, 0, 0 };
	copy.extent = { GetRTGIWidth(), GetRTGIHeight(), 1 };
	copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.dstSubresource.baseArrayLayer = 0;
	copy.dstSubresource.layerCount = 1;
	copy.dstSubresource.mipLevel = 0;
	copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.srcSubresource.baseArrayLayer = 0;
	copy.srcSubresource.layerCount = 1;
	copy.srcSubresource.mipLevel = 0;

	cmdBuffer.CopyImage(spatialDenoisedImage.Get(), VK_IMAGE_LAYOUT_GENERAL, rtgiImage.Get(), VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
}

void DeferredPipeline::PushTAAConstants(const CommandBuffer& cmdBuffer, const Camera* camera)
{
	glm::mat4 view = camera->GetViewMatrix();
	glm::mat4 proj = camera->GetProjectionMatrix();

	TAAConstants constants{};
	constants.projInv = glm::inverse(proj);
	constants.projViewInv = glm::inverse(proj * view);
	constants.prevProjView = prevProj * prevView;
	constants.width = GetRTGIWidth();
	constants.height = GetRTGIHeight();
	constants.maxSampleCount = maxSampleCountTAA;
	
	taaPipeline->PushConstant(cmdBuffer, constants, VK_SHADER_STAGE_COMPUTE_BIT);

	prevView = view;
	prevProj = proj;

	SpatialConstants sConstants{};
	sConstants.width = GetRTGIWidth();
	sConstants.height = GetRTGIHeight();
	sConstants.stepCount = spatialStepCount;

	spatialPipeline->PushConstant(cmdBuffer, sConstants, VK_SHADER_STAGE_COMPUTE_BIT);
}

Skybox* DeferredPipeline::CreateNewSkybox(const std::string& path)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkCommandPool pool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
	VkCommandBuffer cmdBuffer = Vulkan::BeginSingleTimeCommands(pool);

	Skybox* ret = Skybox::ReadFromHDR(path, cmdBuffer);

	Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, cmdBuffer, pool);
	Vulkan::YieldCommandPool(ctx.graphicsIndex, pool);

	return ret;
}

void DeferredPipeline::LoadSkybox(const std::string& path)
{
	skybox.reset(CreateNewSkybox(path));

	skybox->targetUsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	skybox->depthUsageFlags  |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	uint32_t width = framebuffer.GetWidth(), height = framebuffer.GetHeight();
	if (width != 0 && height != 0)
		skybox->Resize(width, height);

	skybox->targetView = GetAlbedoView();
	skybox->depth = framebuffer.GetDepthView();

	rtgiPipeline->BindImageToName("skybox", skybox->GetCubemap()->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void DeferredPipeline::Execute(const Payload& payload, const std::vector<MeshObject*>& objects)
{
	UpdateTextureBuffer(); // temp !!!

	if (Renderer::canRayTrace)
	{
		if (!TLAS->HasBeenBuilt() && !objects.empty())
			TLAS->Build(objects, RTGI_TLAS_INDEX_TYPE, payload.commandBuffer.Get());
		else
			TLAS->Update(objects, RTGI_TLAS_INDEX_TYPE, payload.commandBuffer.Get());
	}

	UpdateUBO(payload.camera);

	const CommandBuffer cmdBuffer = payload.commandBuffer;
	Renderer* renderer = payload.renderer;

	framebuffer.StartRenderPass(cmdBuffer);

	PerformFirstDeferred(cmdBuffer, payload, objects);

	cmdBuffer.BeginDebugUtilsLabel("skybox");

	if (skybox != nullptr)
		skybox->Draw(cmdBuffer, payload.camera);

	cmdBuffer.EndDebugUtilsLabel();

	if (Renderer::canRayTrace)
	{
		SetInstanceData(objects);
		PerformRayTracedRendering(cmdBuffer, payload);
	}

	PerformSecondDeferred(cmdBuffer, payload);

	framebuffer.TransitionFromReadToWrite(cmdBuffer);
}

void DeferredPipeline::SetInstanceData(const std::vector<MeshObject*>& objects)
{
	std::vector<InstanceData> instances;
	instances.reserve(objects.size());

	for (MeshObject* obj : objects)
	{
		const Mesh& mesh = obj->mesh;

		uint32_t vOffset = Renderer::g_vertexBuffer.GetItemOffset(mesh.vertexMemory);
		uint32_t iOffset = Renderer::g_indexBuffer.GetItemOffset(mesh.indexMemory);

		instances.emplace_back(vOffset, iOffset, mesh.GetMaterialIndex());
	}
	std::memcpy(instanceBuffer.GetMappedPointer(), instances.data(), sizeof(InstanceData) * instances.size());
}

void DeferredPipeline::PerformRayTracedRendering(const CommandBuffer& cmdBuffer, const Payload& payload)
{
	cmdBuffer.BeginDebugUtilsLabel("RTGI");

	PushRTGIConstants(payload);

	cmdBuffer.SetCheckpoint(static_cast<const void*>("start of RTGI"));

	rtgiPipeline->Bind(cmdBuffer);
	rtgiPipeline->Execute(cmdBuffer, GetRTGIWidth(), GetRTGIHeight(), 1);

	cmdBuffer.SetCheckpoint(static_cast<const void*>("end of RTGI"));

	cmdBuffer.EndDebugUtilsLabel();

	cmdBuffer.BeginDebugUtilsLabel("TAA");

	TransitionResourcesToTAA(cmdBuffer);
	PushTAAConstants(cmdBuffer, payload.camera);

	taaPipeline->Execute(cmdBuffer, GetRTGIWidth() / 8, GetRTGIHeight() / 8, 1);
	spatialPipeline->Execute(cmdBuffer, GetRTGIWidth() / 8, GetRTGIHeight() / 8, 1);

	CopyDenoisedToRTGI(cmdBuffer);
	CopyResourcesForNextTAA(cmdBuffer);
	TransitionResourcesFromTAA(cmdBuffer);

	cmdBuffer.EndDebugUtilsLabel();
}

void DeferredPipeline::PerformFirstDeferred(const CommandBuffer& cmdBuffer, const Payload& payload, const std::vector<MeshObject*>& objects)
{
	cmdBuffer.BeginDebugUtilsLabel("first deferred");

	firstPipeline->Bind(cmdBuffer);

	Renderer::BindBuffersForRendering(cmdBuffer);

	glm::mat4 view = payload.camera->GetViewMatrix();
	glm::mat4 proj = payload.camera->GetProjectionMatrix();

	PushConstant pushConstant{};
	for (MeshObject* obj : objects)
	{
		glm::mat4 model = obj->transform.GetModelMatrix();

		pushConstant.model = model;
		pushConstant.velocity = payload.camera->GetMotionVector(); // only for static objects, otherwise: obj->transform.GetMotionVector(proj, view, model);
		pushConstant.materialID = obj->mesh.GetMaterialIndex();

		firstPipeline->PushConstant(cmdBuffer, pushConstant, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

		Renderer::RenderMesh(cmdBuffer, obj->mesh);
	}

	cmdBuffer.EndRenderPass();

	cmdBuffer.EndDebugUtilsLabel();
}

void DeferredPipeline::PerformSecondDeferred(const CommandBuffer& cmdBuffer, const Payload& payload)
{
	cmdBuffer.BeginDebugUtilsLabel("second deferred");

	payload.renderer->StartRenderPass(payload.renderer->GetDefault3DRenderPass());

	secondPipeline->Bind(cmdBuffer);

	secondPipeline->PushConstant(cmdBuffer, payload.camera->position, VK_SHADER_STAGE_FRAGMENT_BIT);

	cmdBuffer.Draw(6, 1, 0, 0);

	cmdBuffer.EndRenderPass();

	cmdBuffer.EndDebugUtilsLabel();
}

void DeferredPipeline::Resize(const Payload& payload)
{
	framebuffer.Resize(payload.width, payload.height);

	BindGBuffers();

	skybox->Resize(payload.width, payload.height);
	skybox->targetView = GetAlbedoView();
	skybox->depth = framebuffer.GetDepthView();

	ResizeRTGI(payload.width, payload.height);
	ResizeTAA(payload.width, payload.height);
}

void DeferredPipeline::OnRenderingBufferResize(const Payload& payload)
{
	rtgiPipeline->BindBufferToName("vertexBuffer", Renderer::g_vertexBuffer.GetBufferHandle());
	rtgiPipeline->BindBufferToName("indexBuffer", Renderer::g_indexBuffer.GetBufferHandle());
}

std::vector<RenderPipeline::IntVariable> DeferredPipeline::GetIntVariables()
{
	std::vector<RenderPipeline::IntVariable> ret;
	ret.emplace_back("taa sample count", &maxSampleCountTAA);
	ret.emplace_back("rtgi sample count", &rtgiSampleCount);
	ret.emplace_back("rtgi bounce count", &rtgiBounceCount);
	ret.emplace_back("step count", &spatialStepCount);

	return ret;
}

void DeferredPipeline::UpdateTextureBuffer()
{
	constexpr size_t MAX_PROCESSED_COUNT = 5;

	const size_t pbrSize = Material::pbrTextures.size();
	const size_t maxSize = pbrSize * MAX_PROCESSED_COUNT;

	std::vector<uint64_t>& processedMaterials = processedMats[FIF::frameIndex];

	std::vector<VkDescriptorImageInfo> imageInfos(maxSize);
	std::vector<VkWriteDescriptorSet>  writeSets(maxSize);

	int processedCount = 0;

	if (processedMaterials.size() < Mesh::materials.size())
		processedMaterials.resize(Mesh::materials.size());

	for (int i = 0; i < Mesh::materials.size(); i++)
	{
		if (processedCount >= MAX_PROCESSED_COUNT)
			break;

		if (processedMaterials[i] == Mesh::materials[i].handle)
			continue;

		for (int j = 0; j < pbrSize; j++)
		{
			size_t index = i * pbrSize + j;

			VkDescriptorImageInfo& imageInfo = imageInfos[index];
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = Mesh::materials[i][Material::pbrTextures[j]]->imageView;
			imageInfo.sampler = Renderer::defaultSampler;

			VkWriteDescriptorSet& writeSet = writeSets[index];
			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeSet.descriptorCount = 1;
			writeSet.dstArrayElement = index;
			writeSet.dstBinding = 2;
			writeSet.dstSet = firstPipeline->GetDescriptorSets()[0];
			writeSet.pImageInfo = &imageInfos[index];
		}
		processedMaterials.push_back(Mesh::materials[i].handle);
		processedCount++;
	}

	if (processedCount > 0)
		vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, static_cast<uint32_t>(processedCount * pbrSize), writeSets.data(), 0, nullptr);
}

void DeferredPipeline::SetTextureBuffer()
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = VK_NULL_HANDLE;
	imageInfo.sampler = Renderer::defaultSampler;

	std::vector<VkDescriptorImageInfo> imageInfos(500, imageInfo);

	std::array<VkWriteDescriptorSet, FIF::FRAME_COUNT> writeSets{};

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
	{
		VkWriteDescriptorSet& writeSet = writeSets[i];
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeSet.dstSet = firstPipeline->GetAllDescriptorSets()[i][0];
		writeSet.pImageInfo = imageInfos.data();
		writeSet.descriptorCount = 500;
		writeSet.dstArrayElement = 0;
		writeSet.dstBinding = 2;
	}

	vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, FIF::FRAME_COUNT, writeSets.data(), 0, nullptr);
}

void DeferredPipeline::UpdateUBO(Camera* cam)
{
	UBO* ubo = uboBuffer.GetMappedPointer<UBO>();

	ubo->camPos = glm::vec4(cam->position, 1.0f);
	ubo->proj = cam->GetProjectionMatrix();
	ubo->view = cam->GetViewMatrix();
}

uint32_t DeferredPipeline::GetRTGIWidth() const
{
	return framebuffer.GetWidth() / RTGI_RESOLUTION_UPSCALE;
}

uint32_t DeferredPipeline::GetRTGIHeight() const
{
	return framebuffer.GetHeight() / RTGI_RESOLUTION_UPSCALE;
}

DeferredPipeline::~DeferredPipeline()
{
	vgm::Delete(renderPass);

	vgm::Delete(rtgiView);
	vgm::Delete(prevRtgiView);
	vgm::Delete(prevDepthView);
	vgm::Delete(denoisedRtgiView);
	vgm::Delete(spatialDenoisedView);
}