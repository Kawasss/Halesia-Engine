#include <algorithm>
#include <array>

#include "renderer/Framebuffer.h"
#include "renderer/Vulkan.h"
#include "renderer/GarbageManager.h"

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

	this->images.resize(imageCount + 1); // the depth buffer is stored as the last image in the vector
	this->imageViews.resize(imageCount + 1);
	this->memories.resize(imageCount + 1);
	
	Allocate();
}

void Framebuffer::Init(VkRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<VkFormat>& formats, float relativeRes)
{
	this->renderPass = renderPass;
	this->width   = static_cast<uint32_t>(width * relativeRes);
	this->height  = static_cast<uint32_t>(height * relativeRes);
	this->formats = formats;
	this->relRes  = relRes;

	const size_t imageCount = formats.size();

	this->images.resize(imageCount + 1); // the depth buffer is stored as the last image in the vector
	this->imageViews.resize(imageCount + 1);
	this->memories.resize(imageCount + 1);

	Allocate();
}

void Framebuffer::Allocate()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	for (int i = 0; i < images.size() - 1; i++)
	{
		Vulkan::CreateImage(width, height, 1, 1, formats[i], VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, images[i], memories[i]);
		imageViews[i] = Vulkan::CreateImageView(images[i], VK_IMAGE_VIEW_TYPE_2D, 1, 1, formats[i], VK_IMAGE_ASPECT_COLOR_BIT);
	}

	VkFormat depthFormat = ctx.physicalDevice.GetDepthFormat();
	Vulkan::CreateImage(width, height, 1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, images.back(), memories.back());
	imageViews.back() = Vulkan::CreateImageView(images.back(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.width = width;
	createInfo.height = height;
	createInfo.renderPass = renderPass;
	createInfo.attachmentCount = static_cast<uint32_t>(images.size());
	createInfo.pAttachments = imageViews.data();
	createInfo.layers = 1;

	VkResult result = vkCreateFramebuffer(ctx.logicalDevice, &createInfo, nullptr, &framebuffer);
	CheckVulkanResult("Failed to create a framebuffer", result, vkCreateFramebuffer);

	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);

	TransitionFromUndefinedToWrite(commandBuffer);

	Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, commandBuffer, commandPool);
	Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);
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

void Framebuffer::TransitionFromUndefinedToWrite(CommandBuffer commandBuffer)
{
	constexpr VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	constexpr VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	constexpr VkAccessFlags srcAccess = 0;
	constexpr VkAccessFlags dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	constexpr VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	constexpr VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	for (int i = 0; i < images.size() - 1; i++) // skip the depth buffer !!
		Vulkan::TransitionColorImage(commandBuffer.Get(), images[i], oldLayout, newLayout, srcAccess, dstAccess, srcStage, dstStage);
}

void Framebuffer::TransitionFromReadToWrite(CommandBuffer commandBuffer)
{
	constexpr VkImageLayout oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	constexpr VkImageLayout newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	constexpr VkAccessFlags srcAccess = VK_ACCESS_SHADER_READ_BIT;
	constexpr VkAccessFlags dstAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	constexpr VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	constexpr VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	for (int i = 0; i < images.size() - 1; i++)
		Vulkan::TransitionColorImage(commandBuffer.Get(), images[i], oldLayout, newLayout, srcAccess, dstAccess, srcStage, dstStage);
}

void Framebuffer::TransitionFromWriteToRead(CommandBuffer commandBuffer)
{
	constexpr VkImageLayout oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	constexpr VkImageLayout newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	constexpr VkAccessFlags srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	constexpr VkAccessFlags dstAccess = VK_ACCESS_SHADER_READ_BIT;

	constexpr VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	constexpr VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	for (int i = 0; i < images.size() - 1; i++)
		Vulkan::TransitionColorImage(commandBuffer.Get(), images[i], oldLayout, newLayout, srcAccess, dstAccess, srcStage, dstStage);
}

void Framebuffer::Destroy()
{
	if (framebuffer == VK_NULL_HANDLE)
		return;

	const Vulkan::Context& context = Vulkan::GetContext();

	for (int i = 0; i < images.size(); i++)
	{
		vgm::Delete(images[i]);
		vgm::Delete(imageViews[i]);
		vgm::Delete(memories[i]);
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
	CheckVulkanResult("Failed to create an observer framebuffer", result, vkCreateFramebuffer);
}

void ObserverFramebuffer::Destroy()
{
	if (framebuffer != VK_NULL_HANDLE)
		vgm::Delete(framebuffer);
}