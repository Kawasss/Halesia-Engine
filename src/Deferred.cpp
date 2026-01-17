module;

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include "renderer/Renderer.h"
#include "renderer/Vulkan.h"
#include "renderer/RenderPipeline.h"
#include "renderer/VideoMemoryManager.h"
#include "renderer/Framebuffer.h"
#include "renderer/Buffer.h"
#include "renderer/CommandBuffer.h"

#include "core/MeshObject.h"

module Renderer.Deferred;

import std;

import Core.CameraObject;

import Renderer.SkyPipeline;
import Renderer.ImageTransitioner;
import Renderer.PipelineCreator;
import Renderer.AccelerationStructure;
import Renderer.VulkanGarbageManager;
import Renderer.ComputePipeline;
import Renderer.RayTracingPipeline;
import Renderer.GraphicsPipeline;

struct DeferredPipeline::PushConstant
{
	glm::mat4 model;
	int materialID;
	float uvScale;
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
	InstanceData(float u, uint32_t v, uint32_t i, uint32_t m) : uvScale(u), vertexOffset(v), indexOffset(i), material(m) {}

	float uvScale;
	uint32_t vertexOffset;
	uint32_t indexOffset;
	int material;
};

struct DeferredPipeline::SecondConstants
{
	glm::vec3 camPos;
	int renderMode;
};

constexpr VkFormat GBUFFER_POSITION_FORMAT = VK_FORMAT_R32G32B32A32_SFLOAT; // 32 bit format takes a lot of performance compared to an 8 bit format
constexpr VkFormat GBUFFER_ALBEDO_FORMAT   = VK_FORMAT_R16G16B16A16_UNORM;
constexpr VkFormat GBUFFER_NORMAL_FORMAT   = VK_FORMAT_R16G16B16A16_SFLOAT; // 16 bit instead of 8 bit to allow for negative normals
constexpr VkFormat GBUFFER_MRAO_FORMAT     = VK_FORMAT_R16G16B16A16_UNORM;  // Metallic, Roughness and Ambient Occlusion
constexpr VkFormat GBUFFER_RTGI_FORMAT     = VK_FORMAT_R16G16B16A16_UNORM;
constexpr VkFormat GBUFFER_VELOCITY_FORMAT = VK_FORMAT_R16G16_SFLOAT;
constexpr VkFormat GBUFFER_GNORMAL_FORMAT  = GBUFFER_NORMAL_FORMAT;

constexpr TopLevelAccelerationStructure::InstanceIndexType RTGI_TLAS_INDEX_TYPE = TopLevelAccelerationStructure::InstanceIndexType::Identifier;

constexpr uint32_t RTGI_RESOLUTION_UPSCALE = 1;

void DeferredPipeline::Start(const Payload& payload)
{
	StartSky(payload);

	std::array<VkFormat, GBUFFER_COUNT> formats =
	{
		GBUFFER_POSITION_FORMAT,
		GBUFFER_ALBEDO_FORMAT,
		GBUFFER_NORMAL_FORMAT,
		GBUFFER_MRAO_FORMAT,
		GBUFFER_VELOCITY_FORMAT,
		GBUFFER_GNORMAL_FORMAT,
	};

	CreateBuffers();
	CreateRenderPass(formats);

	for (int i = 0; i < formats.size() + 1; i++)
		framebuffer.SetImageUsage(i, VK_IMAGE_USAGE_SAMPLED_BIT);

	framebuffer.SetImageUsage(formats.size(), VK_IMAGE_USAGE_TRANSFER_SRC_BIT); // the depth buffer is to be transferred for TAA

	framebuffer.Init(renderPass, payload.width, payload.height, formats, 1.0f);

	Vulkan::SetDebugName(framebuffer.Get(), "deferred framebuffer");

	for (int i = 0; i < formats.size(); i++)
		Vulkan::SetDebugName(framebuffer.GetImages()[i].Get(), (std::string("gbuffer") + std::to_string(i) + "_" + string_VkFormat(formats[i])).c_str());

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
		GBUFFER_GNORMAL_FORMAT,
	};

	CreatePipelines(renderPass, payload.renderer->GetDefault3DRenderPass());

	BindResources();

	if (Renderer::canRayTrace)
	{
		CreateAndBindRTGI(payload);

		BindTLAS();
	}

	CreateTAAPipeline();
	CreateTAAResources(payload.width, payload.height);
	BindTAAResources();

	//SetTextureBuffer();
}

void DeferredPipeline::RecreatePipelines(const Payload& payload)
{
	firstPipeline.reset();
	secondPipeline.reset();
	rtgiPipeline.reset();
	taaPipeline.reset();

	CreatePipelines(renderPass, payload.renderer->GetDefault3DRenderPass());
	CreateRTGIPipeline(payload);
	BindRTGIResources();
	BindTLAS();

	CreateTAAPipeline();
	BindTAAResources();

	BindResources();
}

void DeferredPipeline::ReloadShaders(const Payload& payload)
{
	sky.ReloadShaders(payload);
	RecreatePipelines(payload);
}

void DeferredPipeline::CreateRTGIPipeline(const Payload& payload)
{
	rtgiPipeline = std::make_unique<RayTracingPipeline>("shaders/uncompiled/rtgi.rgen", "shaders/uncompiled/rtgi.rchit", "shaders/uncompiled/rtgi.rmiss");

	rtgiPipeline->BindBufferToName("instanceBuffer", instanceBuffer);
	rtgiPipeline->BindBufferToName("vertexBuffer", Renderer::g_vertexBuffer.GetBufferHandle());
	rtgiPipeline->BindBufferToName("indexBuffer", Renderer::g_indexBuffer.GetBufferHandle());

	rtgiPipeline->BindImageToName("transmittanceLUT", sky.GetTransmittanceView(), Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	rtgiPipeline->BindImageToName("latlongMap", sky.GetLatLongView(), Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void DeferredPipeline::CreateAndBindRTGI(const Payload& payload)
{
	CreateRTGIPipeline(payload);

	ResizeRTGI(payload.width, payload.height);
}

void DeferredPipeline::PushRTGIConstants(const Payload& payload)
{
	RTGIConstants constants{};
	constants.frame = frame++;
	constants.sampleCount = rtgiSampleCount;
	constants.bounceCount = rtgiBounceCount;
	constants.position = payload.camera->transform.GetGlobalPosition();

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
	rtgiPipeline->BindImageToName("geometricNormalImage", GetGeometricNormal(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	rtgiPipeline->BindImageToName("mraoImage", GetMRAOView(), Renderer::noFilterSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	secondPipeline->BindImageToName("globalIlluminationImage", rtgiView, Renderer::noFilterSampler, VK_IMAGE_LAYOUT_GENERAL);
}

void DeferredPipeline::CreateBuffers()
{
	constexpr VkDeviceSize MAX_INSTANCE_COUNT = 500ULL;

	instanceBuffer.Init(sizeof(InstanceData) * MAX_INSTANCE_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	instanceBuffer.MapPermanently();
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

void DeferredPipeline::BindResources()
{
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

	Vulkan::SetDebugName(renderPass, "deferred first render pass");
}

void DeferredPipeline::CreatePipelines(VkRenderPass firstPass, VkRenderPass secondPass)
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/uncompiled/deferredFirst.vert";
	createInfo.fragmentShader = "shaders/uncompiled/deferredFirst.frag";
	createInfo.renderPass = firstPass;
	
	firstPipeline = std::make_unique<GraphicsPipeline>(createInfo);

	createInfo.vertexShader   = "shaders/uncompiled/deferredSecond.vert";
	createInfo.fragmentShader = Renderer::canRayTrace ? "shaders/uncompiled/deferredSecondRT.frag" : "shaders/spirv/deferredSecond.frag";
	createInfo.renderPass = secondPass;
	createInfo.noVertices = true;

	secondPipeline = std::make_unique<GraphicsPipeline>(createInfo);
}

void DeferredPipeline::CreateTAAPipeline()
{
	taaPipeline = std::make_unique<ComputeShader>("shaders/uncompiled/taa.comp");
	spatialPipeline = std::make_unique<ComputeShader>("shaders/uncompiled/spatial.comp");
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

void DeferredPipeline::PushTAAConstants(const CommandBuffer& cmdBuffer, const CameraObject* camera)
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

void DeferredPipeline::Execute(const Payload& payload, const std::vector<MeshObject*>& objects)
{
	sky.Execute(payload, objects);

	if (Renderer::canRayTrace)
	{
		if (!TLAS->HasBeenBuilt() && !objects.empty())
			TLAS->Build(objects, RTGI_TLAS_INDEX_TYPE, payload.commandBuffer.Get());
		else
			TLAS->Update(objects, RTGI_TLAS_INDEX_TYPE, payload.commandBuffer.Get());
	}

	const CommandBuffer cmdBuffer = payload.commandBuffer;
	Renderer* renderer = payload.renderer;

	framebuffer.StartRenderPass(cmdBuffer);

	PerformFirstDeferred(cmdBuffer, payload, objects);

	cmdBuffer.BeginDebugUtilsLabel("skybox");

	cmdBuffer.EndDebugUtilsLabel();

	if (Renderer::canRayTrace)
	{
		SetInstanceData(objects);
		PerformRayTracedRendering(cmdBuffer, payload);
	}

	PerformSecondDeferred(cmdBuffer, payload);

	framebuffer.TransitionFromReadToWrite(cmdBuffer);

	sky.Render(payload);
}

void DeferredPipeline::SetInstanceData(const std::vector<MeshObject*>& objects)
{
	std::vector<InstanceData> instances;
	instances.reserve(objects.size());

	for (MeshObject* obj : objects)
	{
		const Mesh& mesh = obj->mesh;

		uint32_t vOffset = static_cast<uint32_t>(Renderer::g_vertexBuffer.GetItemOffset(mesh.vertexMemory));
		uint32_t iOffset = static_cast<uint32_t>(Renderer::g_indexBuffer.GetItemOffset(mesh.indexMemory));

		instances.emplace_back(mesh.uvScale, vOffset, iOffset, mesh.GetMaterialIndex());
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

	bool currentlyCulling = true; // the renderer always resets the cull mode to back faced culling when a render pipeline is called

	PushConstant pushConstant{};
	for (MeshObject* obj : objects)
	{
		if (currentlyCulling != obj->mesh.cullBackFaces)
		{
			currentlyCulling = obj->mesh.cullBackFaces;
			cmdBuffer.SetCullMode(currentlyCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
		}

		glm::mat4 model = obj->transform.GetModelMatrix();

		pushConstant.model = model;
		pushConstant.materialID = obj->mesh.GetMaterialIndex();
		pushConstant.uvScale = obj->mesh.uvScale;

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

	SecondConstants constants{};
	constants.camPos = payload.camera->transform.GetGlobalPosition();
	constants.renderMode = static_cast<std::underlying_type_t<RenderMode>>(renderMode);

	secondPipeline->PushConstant(cmdBuffer, constants, VK_SHADER_STAGE_FRAGMENT_BIT);

	cmdBuffer.Draw(6, 1, 0, 0);

	cmdBuffer.EndRenderPass();

	CopyDeferredDepthToResultDepth(cmdBuffer, payload);

	cmdBuffer.EndDebugUtilsLabel();
}

void DeferredPipeline::CopyDeferredDepthToResultDepth(const CommandBuffer& cmdBuffer, const Payload& payload)
{
	VkImage deferredImage = framebuffer.GetDepthImage();
	VkImage resultImage = payload.renderer->GetFramebuffer().GetDepthImage();

	ImageTransitioner deferredTrans(deferredImage);
	deferredTrans.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	deferredTrans.srcAccess = VK_ACCESS_SHADER_READ_BIT;
	deferredTrans.dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
	deferredTrans.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	deferredTrans.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	deferredTrans.srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	deferredTrans.dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	deferredTrans.width = payload.width;
	deferredTrans.height = payload.height;

	ImageTransitioner resultTrans(resultImage);
	resultTrans.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
	resultTrans.srcAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT; 
	resultTrans.dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
	resultTrans.oldLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	resultTrans.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	resultTrans.srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	resultTrans.dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	resultTrans.width = payload.width;
	resultTrans.height = payload.height;

	deferredTrans.Transition(cmdBuffer);
	resultTrans.Transition(cmdBuffer);

	VkImageCopy copy{};
	copy.dstOffset = { 0, 0 };
	copy.srcOffset = { 0, 0 };
	copy.extent = { payload.width, payload.height, 1 };
	copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	copy.srcSubresource.baseArrayLayer = 0;
	copy.srcSubresource.layerCount = 1;
	copy.srcSubresource.mipLevel = 0;
	copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	copy.dstSubresource.baseArrayLayer = 0;
	copy.dstSubresource.layerCount = 1;
	copy.dstSubresource.mipLevel = 0;
	
	cmdBuffer.CopyImage(deferredImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, resultImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	
	resultTrans.Detransition(cmdBuffer);
	deferredTrans.Detransition(cmdBuffer);
}

void DeferredPipeline::Resize(const Payload& payload)
{
	framebuffer.Resize(payload.width, payload.height);

	BindGBuffers();

	ResizeRTGI(payload.width, payload.height);
	ResizeTAA(payload.width, payload.height);

	ResizeSky(payload);
}

void DeferredPipeline::OnRenderingBufferResize(const Payload& payload)
{
	rtgiPipeline->BindBufferToName("vertexBuffer", Renderer::g_vertexBuffer.GetBufferHandle());
	rtgiPipeline->BindBufferToName("indexBuffer", Renderer::g_indexBuffer.GetBufferHandle());
}

void DeferredPipeline::StartSky(const Payload& payload)
{
	sky.Start(payload);
}

void DeferredPipeline::ResizeSky(const Payload& payload)
{
	sky.Resize(payload);

	rtgiPipeline->BindImageToName("transmittanceLUT", sky.GetTransmittanceView(), Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	rtgiPipeline->BindImageToName("latlongMap", sky.GetLatLongView(), Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

std::vector<RenderPipeline::IntVariable> DeferredPipeline::GetIntVariables()
{
	std::vector<RenderPipeline::IntVariable> ret;
	ret.reserve(4);

	ret.emplace_back("taa sample count", &maxSampleCountTAA);
	ret.emplace_back("rtgi sample count", &rtgiSampleCount);
	ret.emplace_back("rtgi bounce count", &rtgiBounceCount);
	ret.emplace_back("step count", &spatialStepCount);

	return ret;
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