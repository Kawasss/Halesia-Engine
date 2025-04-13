#pragma once
#include <string>

#include "Buffer.h"

class Cubemap;
class GraphicsPipeline;
class CommandBuffer;
class Camera;

class Skybox
{
public:
	static Skybox* ReadFromHDR(const std::string& path, const CommandBuffer& cmdBuffer);

	~Skybox()
	{
		Destroy();
	}

	Skybox(const Skybox&) = delete;
	Skybox& operator=(Skybox&&) = delete;

	void Draw(const CommandBuffer& cmdBuffer, Camera* camera);

	void Resize(uint32_t width, uint32_t height);

	void Destroy();

	VkImageUsageFlags targetUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	VkImageUsageFlags depthUsageFlags  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VkImageView targetView = VK_NULL_HANDLE;
	VkImageView depth = VK_NULL_HANDLE;

	Cubemap* GetCubemap() { return cubemap; }

private:
	Skybox();

	GraphicsPipeline* pipeline = nullptr; // this is extremely wasteful if there are multiple skyboxes at once that arent sharing the pipeline and renderpass
	VkRenderPass renderPass = VK_NULL_HANDLE;

	uint32_t width = 0, height = 0;

	void CreateFramebuffer(uint32_t width, uint32_t height);

	VkFramebuffer framebuffer = VK_NULL_HANDLE;

	Cubemap* cubemap = nullptr;
};