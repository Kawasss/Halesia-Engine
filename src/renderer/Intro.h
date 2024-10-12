#pragma once
#include <vulkan/vulkan.h>
#include <string>

#include "Buffer.h"

class Swapchain;
class Texture;
class GraphicsPipeline;

class Intro
{
public:
	static constexpr float maxSeconds = 3.0f;
	static constexpr float fadeInOutTime = 1.0f;

	void Create(Swapchain* swapchain, std::string imagePath);

	void WriteDataToBuffer(float timeElapsed);
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void Destroy();

private:
	void CreateRenderPass();
	void CreatePipeline();

	Texture* texture;
	Swapchain* swapchain;

	struct Timer;
	Timer* pTimer;
	Buffer uniformBuffer;

	GraphicsPipeline* pipeline;

	VkRenderPass renderPass;
};