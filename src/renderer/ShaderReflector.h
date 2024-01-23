#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "spirv-reflect/spirv_reflect.h"

class PipelineBuilder
{
public:
	SpvReflectShaderModule CreateShaderModule(VkDevice logicalDevice, const std::vector<char>& shaderSource);

private:
	
	void CreateReflectShaderModule(const std::vector<char>& shaderSource);
	//VkVertexInputBindingDescription GetVertexInputBindingDescription(const std::vector<SpvReflectInterfaceVariable*>& inputVariables);
	//std::vector<VkVertexInputAttributeDescription> GetVertexInputAttributeDescription(const std::vector<SpvReflectInterfaceVariable*>& inputVariables);
};

class ShaderReflector
{
public:
	ShaderReflector(const std::vector<char>& sourceCode);
	~ShaderReflector();

	VkDescriptorSetLayoutBinding GetDescriptorSetLayoutBinding(uint32_t bindingIndex, uint32_t setIndex = 0); // the set is by default 0 in GLSL

private:
	SpvReflectShaderModule module{};
};