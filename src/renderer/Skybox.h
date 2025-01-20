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

	void Draw(const CommandBuffer& cmdBuffer);

	void Destroy();

	VkImageView targetView = VK_NULL_HANDLE;
	VkImageView depth = VK_NULL_HANDLE;

private:
	Skybox();

	void CreateFramebuffer();

	VkFramebuffer framebuffer = VK_NULL_HANDLE;

	Cubemap* cubemap = nullptr;
};