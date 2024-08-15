#pragma once
#include <string>
#include <vector>
#include <map>
#include <vulkan/vulkan.h>

#include "PipelineCreator.h"

class ShaderGroupReflector;
class Swapchain;

class GraphicsPipeline
{
public:
	GraphicsPipeline(const std::string& vertPath, const std::string& fragPath, PipelineOptions flags, VkRenderPass renderPass);
	~GraphicsPipeline();

	GraphicsPipeline(const GraphicsPipeline&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

	void Bind(VkCommandBuffer commandBuffer);

	void BindBufferToName(const std::string& name, VkBuffer buffer);
	void BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout);

	std::vector<VkDescriptorSet>& GetDescriptorSets() { return descriptorSets; }

	VkPipelineLayout GetLayout() { return layout; }

private:
	struct BindingLayout
	{
		uint32_t set;
		VkDescriptorSetLayoutBinding binding;
	};

	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayout(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(uint32_t amount);

	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateGraphicsPipeline(const std::vector<std::vector<char>>& shaders, PipelineOptions flags, VkRenderPass renderPass);

	std::map<std::string, BindingLayout> nameToLayout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::vector<VkDescriptorSet> descriptorSets;
};