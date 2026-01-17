module;

#include <vulkan/vulkan.h>

#include "renderer/Renderer.h"
#include "renderer/Vulkan.h"
#include "renderer/RenderPipeline.h"
#include "renderer/CommandBuffer.h"
#include "renderer/Texture.h"

module Renderer.SkyPipeline;

import std;

import Renderer.VulkanGarbageManager;
import Renderer.GraphicsPipeline;

constexpr VkFormat LUT_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr uint32_t LUT_FORMAT_SIZE = sizeof(uint16_t) * 4;
constexpr VkImageUsageFlags LUT_USAGE = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

constexpr uint32_t TRANSMITTANCE_LUT_WIDTH  = 256;
constexpr uint32_t TRANSMITTANCE_LUT_HEIGHT = 64;

constexpr uint32_t M_SCATTERING_LUT_WIDTH  = 32;
constexpr uint32_t M_SCATTERING_LUT_HEIGHT = M_SCATTERING_LUT_WIDTH;

constexpr uint32_t LATLONG_MAP_WIDTH  = 200;
constexpr uint32_t LATLONG_MAP_HEIGHT = LATLONG_MAP_WIDTH;

void SkyPipeline::Start(const Payload& payload)
{
	ReloadShaders(payload);
	CreateImages(payload.commandBuffer, payload.width, payload.height);
}

void SkyPipeline::Execute(const Payload& payload, const std::vector<MeshObject*>& objects)
{
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	cmdBuffer.BeginDebugUtilsLabel("transmittance LUT");

	BeginRenderPass(cmdBuffer, transmittanceLUT);

	//payload.renderer->SetViewport(cmdBuffer, { TRANSMITTANCE_LUT_WIDTH, TRANSMITTANCE_LUT_HEIGHT });

	transmittancePipeline->Bind(cmdBuffer);
	cmdBuffer.Draw(6, 1, 0, 0);

	EndRenderPass(cmdBuffer, transmittanceLUT);

	cmdBuffer.EndDebugUtilsLabel();
	cmdBuffer.BeginDebugUtilsLabel("mscattering LUT");

	BeginRenderPass(cmdBuffer, mscatteringLUT);

	//payload.renderer->SetViewport(cmdBuffer, { M_SCATTERING_LUT_WIDTH, M_SCATTERING_LUT_HEIGHT });

	mscatteringPipeline->Bind(cmdBuffer);
	cmdBuffer.Draw(6, 1, 0, 0);

	EndRenderPass(cmdBuffer, mscatteringLUT);

	cmdBuffer.EndDebugUtilsLabel();
	cmdBuffer.BeginDebugUtilsLabel("latlong map");

	BeginRenderPass(cmdBuffer, latlongMap);

	//payload.renderer->SetViewport(cmdBuffer, { LATLONG_MAP_WIDTH, LATLONG_MAP_HEIGHT });

	latlongPipeline->Bind(cmdBuffer);
	cmdBuffer.Draw(6, 1, 0, 0);

	EndRenderPass(cmdBuffer, latlongMap);

	cmdBuffer.EndDebugUtilsLabel();
	//cmdBuffer.BeginDebugUtilsLabel("rendering");

	//BeginPresentationRenderPass(cmdBuffer, payload.renderer, payload.width, payload.height);

	//payload.renderer->SetViewport(cmdBuffer, { payload.width, payload.height });

	//atmospherePipeline->Bind(cmdBuffer);
	//cmdBuffer.Draw(6, 1, 0, 0);

	//cmdBuffer.EndRendering();

	//cmdBuffer.EndDebugUtilsLabel();

	transmittanceLUT.TransitionTo(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cmdBuffer);
	mscatteringLUT.TransitionTo(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cmdBuffer);
	latlongMap.TransitionTo(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cmdBuffer);
}

void SkyPipeline::Render(const Payload& payload)
{
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	cmdBuffer.BeginDebugUtilsLabel("rendering");

	BeginPresentationRenderPass(cmdBuffer, payload.renderer, payload.width, payload.height);

	atmospherePipeline->Bind(cmdBuffer);
	cmdBuffer.Draw(6, 1, 0, 0);

	cmdBuffer.EndRendering();

	cmdBuffer.EndDebugUtilsLabel();
}

void SkyPipeline::BeginRenderPass(const CommandBuffer& cmdBuffer, Image& image)
{
	BeginRenderPass(cmdBuffer, image.view, image.GetWidth(), image.GetHeight());
}

void SkyPipeline::BeginRenderPass(const CommandBuffer& cmdBuffer, VkImageView view, uint32_t width, uint32_t height)
{
	VkRenderingAttachmentInfo attachInfo{};
	attachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachInfo.imageView = view;
	attachInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	attachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachInfo.clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.layerCount = 1;
	renderInfo.pColorAttachments = &attachInfo;
	renderInfo.pDepthAttachment = nullptr;
	renderInfo.pStencilAttachment = nullptr;
	renderInfo.renderArea = { VkOffset2D{ 0, 0 }, VkExtent2D{ width, height } };

	cmdBuffer.BeginRendering(renderInfo);
}

void SkyPipeline::EndRenderPass(const CommandBuffer& cmdBuffer, Image& image)
{
	cmdBuffer.EndRendering();
	image.TransitionTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cmdBuffer);
}

void SkyPipeline::BeginPresentationRenderPass(const CommandBuffer& cmdBuffer, Renderer* renderer, uint32_t width, uint32_t height)
{
	VkRenderingAttachmentInfo attachInfo{};
	attachInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	attachInfo.imageView = renderer->GetFramebuffer().GetViews()[0];
	attachInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	attachInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthInfo{};
	depthInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthInfo.imageView = renderer->GetFramebuffer().GetDepthView();
	depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	depthInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.layerCount = 1;
	renderInfo.pColorAttachments = &attachInfo;
	renderInfo.pDepthAttachment = &depthInfo;
	renderInfo.pStencilAttachment = nullptr;
	renderInfo.renderArea = { VkOffset2D{ 0, 0 }, VkExtent2D{ width, height } };

	cmdBuffer.BeginRendering(renderInfo);
}

void SkyPipeline::ReloadShaders(const Payload& payload)
{
	CreatePipelines();
}

void SkyPipeline::Resize(const Payload& payload)
{
	CreateImages(payload.commandBuffer, payload.width, payload.height);
}

void SkyPipeline::CreateImages(const CommandBuffer& cmdBuffer, uint32_t width, uint32_t height)
{
	transmittanceLUT.Destroy();
	mscatteringLUT.Destroy();
	latlongMap.Destroy();

	transmittanceLUT.Create(width/*TRANSMITTANCE_LUT_WIDTH*/, height/*TRANSMITTANCE_LUT_HEIGHT*/, 1, LUT_FORMAT, LUT_FORMAT_SIZE, LUT_USAGE, Image::None);
	mscatteringLUT.Create(width/*M_SCATTERING_LUT_WIDTH*/, height/*M_SCATTERING_LUT_HEIGHT*/, 1, LUT_FORMAT, LUT_FORMAT_SIZE, LUT_USAGE, Image::None);
	latlongMap.Create(width/*LATLONG_MAP_WIDTH*/, height/*LATLONG_MAP_HEIGHT*/, 1, LUT_FORMAT, LUT_FORMAT_SIZE, LUT_USAGE, Image::None);

	Vulkan::SetDebugName(transmittanceLUT.image.Get(), "transmittance LUT");
	Vulkan::SetDebugName(mscatteringLUT.image.Get(), "mscattering LUT");
	Vulkan::SetDebugName(latlongMap.image.Get(), "latlong map");

	transmittanceLUT.TransitionTo(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cmdBuffer);
	mscatteringLUT.TransitionTo(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cmdBuffer);
	latlongMap.TransitionTo(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, cmdBuffer);

	bindImagesToPipelines();
}

void SkyPipeline::CreatePipelines()
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader = "shaders/uncompiled/atmosphere.vert";
	createInfo.fragmentShader = "shaders/uncompiled/transmittanceLUT.frag";
	createInfo.renderPass = VK_NULL_HANDLE;
	createInfo.colorFormats.push_back(LUT_FORMAT);

	transmittancePipeline = std::make_unique<GraphicsPipeline>(createInfo);

	createInfo.fragmentShader = "shaders/uncompiled/mscatteringLUT.frag";
	mscatteringPipeline = std::make_unique<GraphicsPipeline>(createInfo);

	createInfo.fragmentShader = "shaders/uncompiled/latlongMap.frag";
	latlongPipeline = std::make_unique<GraphicsPipeline>(createInfo);

	createInfo.colorFormats[0] = VK_FORMAT_R16G16B16A16_UNORM;
	createInfo.depthStencilFormat = Vulkan::GetContext().physicalDevice.GetDepthFormat();
	createInfo.fragmentShader = "shaders/uncompiled/atmosRender.frag";
	atmospherePipeline = std::make_unique<GraphicsPipeline>(createInfo);
}

void SkyPipeline::bindImagesToPipelines()
{
	mscatteringPipeline->BindImageToName("transmittanceLUT", transmittanceLUT.view, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	latlongPipeline->BindImageToName("transmittanceLUT", transmittanceLUT.view, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	latlongPipeline->BindImageToName("mscatteringLUT", mscatteringLUT.view, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	atmospherePipeline->BindImageToName("transmittanceLUT", transmittanceLUT.view, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	atmospherePipeline->BindImageToName("latlongMap", latlongMap.view, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

VkImageView SkyPipeline::GetTransmittanceView() const
{
	return transmittanceLUT.view;
}

VkImageView SkyPipeline::GetMScatteringView() const
{
	return mscatteringLUT.view;
}

VkImageView SkyPipeline::GetLatLongView() const
{
	return latlongMap.view;
}