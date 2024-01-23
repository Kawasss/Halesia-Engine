#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include "spirv-reflect/spirv_reflect.h"

class ShaderReflector
{
public:
	ShaderReflector(const std::vector<char>& sourceCode);
	~ShaderReflector();

	VkDescriptorSetLayoutBinding GetDescriptorSetLayoutBinding(uint32_t bindingIndex, uint32_t setIndex = 0); // the set is by default 0 in GLSL

private:
	SpvReflectShaderModule module{};
};

class ShaderGroupReflector
{
public:
	ShaderGroupReflector(const std::vector<std::vector<char>>& sourceCodes);
	~ShaderGroupReflector();

	std::vector<VkDescriptorSetLayoutBinding> GetLayoutBindingsOfSet(uint32_t setIndex);


private:
	std::vector<SpvReflectShaderModule> modules;
};