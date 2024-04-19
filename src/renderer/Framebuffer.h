#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class Framebuffer
{
public:
	Framebuffer(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, float relativeRes = 1.0f);
	~Framebuffer();

	void Resize(uint32_t width, uint32_t height);

	operator VkFramebuffer() { return framebuffer; }

private:
	void Destroy();
	void Allocate();

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	uint32_t width = 0, height = 0;
	float relRes = 1.0f;

	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	std::vector<VkDeviceMemory> memories;
};