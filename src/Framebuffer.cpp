#include "renderer/Framebuffer.h"
#include "renderer/Vulkan.h"

Framebuffer::Framebuffer(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, float relativeRes) : renderPass(renderPass), width(width), height(height), relRes(relRes)
{
	this->images.resize(imageCount);
	this->imageViews.resize(imageCount);
	this->memories.resize(imageCount);
	Allocate();
}

Framebuffer::~Framebuffer()
{
	Destroy();
	images.clear();
	imageViews.clear();
	memories.clear();
}

void Framebuffer::Allocate()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	for (int i = 0; i < images.size(); i++)
	{
		Vulkan::CreateImage(width, height, 1, 1, VK_FORMAT_R8G8B8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, images[i], memories[i]);
		Vulkan::CreateImageView(images[i], VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.width = width;
	createInfo.height = height;
	createInfo.renderPass = renderPass;
	createInfo.attachmentCount = images.size();
	createInfo.pAttachments = imageViews.data();
	createInfo.layers = 1;

	VkResult result = vkCreateFramebuffer(context.logicalDevice, &createInfo, nullptr, &framebuffer);
	CheckVulkanResult("Failed to create a framebuffer", result, vkCreateFramebuffer);
}

void Framebuffer::Resize(uint32_t width, uint32_t height)
{
	this->width = width;
	this->height = height;
	Destroy();
	Allocate();
}

void Framebuffer::Destroy()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	for (int i = 0; i < images.size(); i++)
	{
		vkDestroyImage(context.logicalDevice, images[i], nullptr);
		vkDestroyImageView(context.logicalDevice, imageViews[i], nullptr);
		vkFreeMemory(context.logicalDevice, memories[i], nullptr);
	}

	vkDestroyFramebuffer(context.logicalDevice, framebuffer, nullptr);
}