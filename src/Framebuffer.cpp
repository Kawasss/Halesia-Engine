module;

#include "renderer/Vulkan.h"
#include "renderer/VulkanAPIError.h"

module Renderer.Framebuffer;

import std;

import Renderer.VulkanGarbageManager;

Framebuffer::Framebuffer(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, VkFormat format, float relativeRes)
{
	Init(renderPass, imageCount, width, height, format, relativeRes);
}

Framebuffer::~Framebuffer()
{
	Destroy();
}

void Framebuffer::Init(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, VkFormat format, float relativeRes)
{
	this->renderPass = renderPass;
	this->width  = static_cast<uint32_t>(width * relativeRes);
	this->height = static_cast<uint32_t>(height * relativeRes);
	this->relRes = relRes;

	formats.resize(imageCount);
	std::fill(formats.begin(), formats.end(), format);

	ResizeImageContainers(imageCount, true);
	Allocate();
}

void Framebuffer::Init(VkRenderPass renderPass, uint32_t width, uint32_t height, const std::span<VkFormat>& formats, float relativeRes)
{
	this->renderPass = renderPass;
	this->width = static_cast<uint32_t>(width * relativeRes);
	this->height = static_cast<uint32_t>(height * relativeRes);
	this->formats = std::vector<VkFormat>(formats.begin(), formats.end());
	this->relRes = relRes;

	ResizeImageContainers(formats.size(), true);
	Allocate();
}

void Framebuffer::ResizeImageContainers(size_t size, bool depth)
{
	if (depth)
		size++; // the depth buffer is stored as the last image in the vector

	images.resize(size);
	imageViews.resize(size);

	if (imageUsages.size() < size)
		imageUsages.resize(size);
}

void Framebuffer::Allocate()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	for (int i = 0; i < images.size() - 1; i++)
	{
		images[i] = Vulkan::CreateImage(width, height, 1, 1, formats[i], VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | imageUsages[i], VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
		imageViews[i] = Vulkan::CreateImageView(images[i].Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, formats[i], VK_IMAGE_ASPECT_COLOR_BIT);
	}

	VkFormat depthFormat = ctx.physicalDevice.GetDepthFormat();
	images.back() = Vulkan::CreateImage(width, height, 1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | imageUsages.back(), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
	imageViews.back() = Vulkan::CreateImageView(images.back().Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.width = width;
	createInfo.height = height;
	createInfo.renderPass = renderPass;
	createInfo.attachmentCount = static_cast<uint32_t>(images.size());
	createInfo.pAttachments = imageViews.data();
	createInfo.layers = 1;

	VkResult result = vkCreateFramebuffer(ctx.logicalDevice, &createInfo, nullptr, &framebuffer);
	CheckVulkanResult("Failed to create a framebuffer", result);

	Vulkan::ExecuteSingleTimeCommands([this](const CommandBuffer& cmdBuffer)
		{
			TransitionFromUndefinedToWrite(cmdBuffer);
		}
	);
}

void Framebuffer::SetImageUsage(size_t index, VkImageUsageFlags flags)
{
	if (index >= imageUsages.size())
		imageUsages.resize(index + 1);

	imageUsages[index] |= flags;
}

VkImageUsageFlags Framebuffer::GetImageUsage(size_t index) const
{
	return index > imageUsages.size() ? 0 : imageUsages[index];
}

void Framebuffer::Resize(uint32_t width, uint32_t height)
{
	this->width  = static_cast<uint32_t>(width * relRes);
	this->height = static_cast<uint32_t>(height * relRes);

	Destroy();
	Allocate();
}

void Framebuffer::StartRenderPass(CommandBuffer commandBuffer)
{
	constexpr VkClearValue baseClear = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	std::vector<VkClearValue> clearValues(images.size(), baseClear);
	clearValues.back().depthStencil = { 1, 0 };

	VkRenderPassBeginInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.framebuffer = framebuffer;
	info.renderPass = renderPass;
	info.renderArea.offset = { 0, 0 }; // maybe make this modifiable
	info.renderArea.extent = { width, height };
	info.clearValueCount = static_cast<uint32_t>(clearValues.size());
	info.pClearValues = clearValues.data();

	commandBuffer.BeginRenderPass(info, VK_SUBPASS_CONTENTS_INLINE);
}

void Framebuffer::SetDebugName(const char* name)
{
	Vulkan::SetDebugName(framebuffer, name);
}

void Framebuffer::TransitionFromUndefinedToWrite(const CommandBuffer& commandBuffer)
{
	constexpr VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	constexpr VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	constexpr VkAccessFlags srcAccess = 0;
	constexpr VkAccessFlags dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	constexpr VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	constexpr VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	for (int i = 0; i < images.size() - 1; i++) // skip the depth buffer !!
		Vulkan::TransitionColorImage(commandBuffer.Get(), images[i].Get(), oldLayout, newLayout, srcAccess, dstAccess, srcStage, dstStage);
}

void Framebuffer::TransitionFromReadToWrite(const CommandBuffer& commandBuffer)
{
	for (int i = 0; i < images.size() - 1; i++)
		TransitionImageFromReadToWrite(commandBuffer, i);
}

void Framebuffer::TransitionFromWriteToRead(const CommandBuffer& commandBuffer)
{
	for (int i = 0; i < images.size() - 1; i++)
		TransitionImageFromWriteToRead(commandBuffer, i);
}

void Framebuffer::TransitionImageFromReadToWrite(const CommandBuffer& commandBuffer, size_t index)
{
	constexpr VkImageLayout oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	constexpr VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	constexpr VkAccessFlags srcAccess = VK_ACCESS_SHADER_READ_BIT;
	constexpr VkAccessFlags dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	constexpr VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	constexpr VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	TransitionImage(commandBuffer, index, oldLayout, newLayout, srcAccess, dstAccess, srcStage, dstStage);
}

void Framebuffer::TransitionImageFromWriteToRead(const CommandBuffer& commandBuffer, size_t index)
{
	constexpr VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	constexpr VkImageLayout newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	constexpr VkAccessFlags srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	constexpr VkAccessFlags dstAccess = VK_ACCESS_SHADER_READ_BIT;

	constexpr VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	constexpr VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	TransitionImage(commandBuffer, index, oldLayout, newLayout, srcAccess, dstAccess, srcStage, dstStage);
}

void Framebuffer::TransitionImage(const CommandBuffer& commandBuffer, size_t index, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
	if (index >= images.size())
		return; // failure

	Vulkan::TransitionColorImage(commandBuffer.Get(), images[index].Get(), oldLayout, newLayout, srcAccess, dstAccess, srcStage, dstStage);
}

void Framebuffer::Destroy()
{
	if (framebuffer == VK_NULL_HANDLE)
		return;

	for (int i = 0; i < images.size(); i++)
	{
		vgm::Delete(imageViews[i]);
		images[i].Destroy();
	}

	vgm::Delete(framebuffer);

	framebuffer = VK_NULL_HANDLE;
}

void ObserverFramebuffer::Observe(const std::vector<VkImageView>& views, uint32_t width, uint32_t height, VkRenderPass renderPass)
{
	Destroy();
	CreateFramebuffer(views.data(), static_cast<uint32_t>(views.size()), width, height, renderPass);
}

void ObserverFramebuffer::Observe(VkImageView view, VkImageView depth, uint32_t width, uint32_t height, VkRenderPass renderPass)
{
	std::array<VkImageView, 2> views = { view, depth };
	
	Destroy();
	CreateFramebuffer(views.data(), 2, width, height, renderPass);
}

void ObserverFramebuffer::CreateFramebuffer(const VkImageView* pViews, uint32_t count, uint32_t width, uint32_t height, VkRenderPass renderPass)
{
	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.attachmentCount = count;
	createInfo.pAttachments = pViews;
	createInfo.width = width;
	createInfo.height = height;
	createInfo.renderPass = renderPass;
	createInfo.layers = 1;

	VkResult result = vkCreateFramebuffer(Vulkan::GetContext().logicalDevice, &createInfo, nullptr, &framebuffer);
	CheckVulkanResult("Failed to create an observer framebuffer", result);
}

void ObserverFramebuffer::Destroy()
{
	if (framebuffer != VK_NULL_HANDLE)
		vgm::Delete(framebuffer);
}