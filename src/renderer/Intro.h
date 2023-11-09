#pragma once
#include "Vulkan.h"
#include <string>

class Swapchain;
class Texture;

class Intro
{
public:
	static constexpr float maxSeconds = 3.0f;
	static constexpr float fadeInOutTime = 1.0f;

	void Create(VulkanCreationObject& creationObject, Swapchain* swapchain, std::string imagePath);

	void WriteDataToBuffer(float timeElapsed);
	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void Destroy();

private:
	std::vector<char> ReadFile(const std::string& path);

	Texture* texture;
	Swapchain* swapchain;
	VkDevice logicalDevice;

	// maybe better to create all of these statically, so that every intro uses the same resources instead of pointlessly making new isntances for everything

	void* uniformBufferPointer;
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;

	VkDescriptorSet descriptorSet;
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout setLayout;

	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	VkRenderPass renderPass;
};