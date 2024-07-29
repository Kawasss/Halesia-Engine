#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "PipelineCreator.h"

class ShaderGroupReflector;
class Swapchain;

class GraphicsPipeline
{
public:
	GraphicsPipeline(const std::string& vertPath, const std::string& fragPath, PipelineFlags flags, Swapchain* swapchain, VkRenderPass renderPass);
	~GraphicsPipeline();

	GraphicsPipeline(const GraphicsPipeline&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

	void Bind(VkCommandBuffer commandBuffer);

	std::vector<VkDescriptorSet>& GetDescriptorSets() { return descriptorSets; }

private:
	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayout(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(uint32_t amount);

	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateGraphicsPipeline(const std::vector<std::vector<char>>& shaders, PipelineFlags flags, Swapchain* swapchain, VkRenderPass renderPass);

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;

	//VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::vector<VkDescriptorSet> descriptorSets;
};