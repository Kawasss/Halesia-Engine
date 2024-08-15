#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class Framebuffer
{
public:
	Framebuffer() = default;
	Framebuffer(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, float relativeRes = 1.0f);
	~Framebuffer();

	void Init(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, float relativeRes = 1.0f);

	void Resize(uint32_t width, uint32_t height);

	VkFramebuffer Get() { return framebuffer; }
	VkRenderPass GetRenderPass() { return renderPass; }

	std::vector<VkImage>& GetImages()    { return images; }
	std::vector<VkImageView>& GetViews() { return imageViews; }

	uint32_t GetWidth()  { return width; }
	uint32_t GetHeight() { return height; }

	void SetDebugName(const char* name);

	void TransitionFromReadToWrite(VkCommandBuffer commandBuffer);
	void TransitionFromWriteToRead(VkCommandBuffer commandBuffer);

private:
	void TransitionFromUndefinedToWrite(VkCommandBuffer commandBuffer);

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