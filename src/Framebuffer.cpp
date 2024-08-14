#include "renderer/Framebuffer.h"
#include "renderer/Vulkan.h"

Framebuffer::Framebuffer(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, float relativeRes)
{
	Init(renderPass, imageCount, width, height, relativeRes);
}

Framebuffer::~Framebuffer()
{
	Destroy();
}

void Framebuffer::Init(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, float relativeRes)
{
	this->renderPass = renderPass;
	this->width  = static_cast<uint32_t>(width * relativeRes);
	this->height = static_cast<uint32_t>(height * relativeRes);
	this->relRes = relRes;

	this->images.resize(imageCount + 1); // the depth buffer is stored as the last image in the vector
	this->imageViews.resize(imageCount + 1);
	this->memories.resize(imageCount + 1);
	
	Allocate();
}

void Framebuffer::Allocate()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	for (int i = 0; i < images.size() - 1; i++)
	{
		Vulkan::CreateImage(width, height, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, images[i], memories[i]);
		imageViews[i] = Vulkan::CreateImageView(images[i], VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	VkFormat depthFormat = context.physicalDevice.GetDepthFormat();
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

	VkResult result = vkCreateFramebuffer(context.logicalDevice, &createInfo, nullptr, &framebuffer);
	CheckVulkanResult("Failed to create a framebuffer", result, vkCreateFramebuffer);
}

void Framebuffer::Resize(uint32_t width, uint32_t height)
{
	this->width  = static_cast<uint32_t>(width * relRes);
	this->height = static_cast<uint32_t>(height * relRes);
	Destroy();
	Allocate();
}

void Framebuffer::SetDebugName(const char* name)
{
	Vulkan::SetDebugName(framebuffer, name);
}

void Framebuffer::Destroy()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	if (framebuffer == VK_NULL_HANDLE)
		return;

	for (int i = 0; i < images.size(); i++)
	{
		vkDestroyImage(context.logicalDevice, images[i], nullptr);
		vkDestroyImageView(context.logicalDevice, imageViews[i], nullptr);
		vkFreeMemory(context.logicalDevice, memories[i], nullptr);
	}

	vkDestroyFramebuffer(context.logicalDevice, framebuffer, nullptr);

	framebuffer = VK_NULL_HANDLE;
}