#pragma once
#include <vulkan/vulkan.h>

class AnimationManager
{
public:
	static AnimationManager* Get();

	void Compute(VkCommandBuffer commandBuffer);
	void Destroy();

private:
	void Create();
	void CreateShader();

	VkDevice logicalDevice = VK_NULL_HANDLE;

	VkPipeline computePipeline = VK_NULL_HANDLE;
	VkDescriptorSetLayout computeSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout computeLayout = VK_NULL_HANDLE;
	VkQueue computeQueue = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
};