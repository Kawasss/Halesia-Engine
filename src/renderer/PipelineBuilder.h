#pragma once
#include <vector>
#include <vulkan/vulkan.h>

class PipelineBuilder
{
public:
	VkPipeline BuildGraphicsPipeline(VkDevice logicalDevice, VkDescriptorSetLayout descriptorSetLayout, VkRenderPass renderPass, VkPipelineLayout& pipelineLayout, std::vector<char>& vertexShaderSource, std::vector<char> fragmentShaderSource, std::vector<VkDynamicState> dynamicStates, VkViewport viewport, VkRect2D scissor);

private:
	VkShaderModule CreateShaderModule(VkDevice logicalDevice, const std::vector<char>& shaderSource);
	void CreateReflectShaderModule(const std::vector<char>& shaderSource);
	//VkVertexInputBindingDescription GetVertexInputBindingDescription(const std::vector<SpvReflectInterfaceVariable*>& inputVariables);
	//std::vector<VkVertexInputAttributeDescription> GetVertexInputAttributeDescription(const std::vector<SpvReflectInterfaceVariable*>& inputVariables);
};