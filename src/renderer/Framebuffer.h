#pragma once
#include <vulkan/vulkan.h>
#include <vector>

#include "CommandBuffer.h"
#include "VideoMemoryManager.h"

class Framebuffer
{
public:
	Framebuffer() = default;
	Framebuffer(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, VkFormat format, float relativeRes = 1.0f);
	~Framebuffer();

	void Init(VkRenderPass renderPass, uint32_t imageCount, uint32_t width, uint32_t height, VkFormat format, float relativeRes = 1.0f);
	void Init(VkRenderPass renderPass, uint32_t width, uint32_t height, const std::vector<VkFormat>& formats, float relativeRes = 1.0f);
	
	template<size_t count>
	void Init(VkRenderPass renderPass, uint32_t width, uint32_t height, const std::array<VkFormat, count>& formats, float relativeRes = 1.0f);

	void Resize(uint32_t width, uint32_t height);

	void StartRenderPass(CommandBuffer commandBuffer);

	VkFramebuffer Get()          const { return framebuffer; }
	VkRenderPass GetRenderPass() const { return renderPass;  }

	std::vector<vvm::Image>& GetImages()   { return images;     }
	std::vector<VkImageView>& GetViews() { return imageViews; }

	uint32_t GetWidth()  const { return width;  }
	uint32_t GetHeight() const { return height; }

	VkImageView GetDepthView() { return imageViews.back(); }

	void SetDebugName(const char* name);

	void TransitionFromReadToWrite(CommandBuffer commandBuffer);
	void TransitionFromWriteToRead(CommandBuffer commandBuffer);

private:
	void Init(VkRenderPass renderPass, uint32_t width, uint32_t height, const VkFormat* pFormats, size_t formatCount, float relativeRes = 1.0f);

	void TransitionFromUndefinedToWrite(CommandBuffer commandBuffer);
	 
	void Destroy();
	void Allocate();

	void ResizeImageContainers(size_t size, bool depth);

	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	std::vector<VkFormat> formats;
	uint32_t width = 0, height = 0;
	float relRes = 1.0f;

	std::vector<vvm::Image> images;
	std::vector<VkImageView> imageViews;
};

class ObserverFramebuffer
{
public:
	ObserverFramebuffer() = default;
	~ObserverFramebuffer() { Destroy(); }

	void Observe(const std::vector<VkImageView>& views, uint32_t width, uint32_t height, VkRenderPass renderPass); // depth buffer is the last element
	void Observe(VkImageView view, VkImageView depth, uint32_t width, uint32_t height, VkRenderPass renderPass);

	void Destroy();

private:
	void CreateFramebuffer(const VkImageView* pViews, uint32_t count, uint32_t width, uint32_t height, VkRenderPass renderPass);

	VkFramebuffer framebuffer;
};

template<size_t count>
void Framebuffer::Init(VkRenderPass renderPass, uint32_t width, uint32_t height, const std::array<VkFormat, count>& formats, float relativeRes)
{
	this->Init(renderPass, width, height, formats.data(), count, relativeRes);
}