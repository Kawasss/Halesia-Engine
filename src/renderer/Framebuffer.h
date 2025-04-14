#pragma once
#include <vulkan/vulkan.h>
#include <span>
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
	void Init(VkRenderPass renderPass, uint32_t width, uint32_t height, const std::span<VkFormat>& formats, float relativeRes = 1.0f);

	void SetImageUsage(size_t index, VkImageUsageFlags flags);
	VkImageUsageFlags GetImageUsage(size_t index) const;

	void Resize(uint32_t width, uint32_t height);

	void StartRenderPass(CommandBuffer commandBuffer);

	VkFramebuffer Get()          const { return framebuffer; }
	VkRenderPass GetRenderPass() const { return renderPass;  }

	std::vector<vvm::Image>& GetImages() { return images;     }
	std::vector<VkImageView>& GetViews() { return imageViews; }

	uint32_t GetWidth()  const { return width;  }
	uint32_t GetHeight() const { return height; }

	size_t GetImageCount() const { return images.size(); }

	VkImageView GetDepthView()  const { return imageViews.back();   }
	VkImage     GetDepthImage() const { return images.back().Get(); }

	void SetDebugName(const char* name);

	void TransitionFromReadToWrite(const CommandBuffer& commandBuffer);
	void TransitionFromWriteToRead(const CommandBuffer& commandBuffer);

	void TransitionImageFromReadToWrite(const CommandBuffer& commandBuffer, size_t index);
	void TransitionImageFromWriteToRead(const CommandBuffer& commandBuffer, size_t index);

	void TransitionImage(const CommandBuffer& commandBuffer, size_t index, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags srcAccess, VkAccessFlags dstAccess, VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage);

private:
	void TransitionFromUndefinedToWrite(const CommandBuffer& commandBuffer);
	 
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
	std::vector<VkImageUsageFlags> imageUsages;
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