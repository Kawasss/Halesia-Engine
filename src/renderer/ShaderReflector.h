#pragma once
#include <set>
#include <vector>
#include <vulkan/vulkan.h>
#include "spirv-reflect/spirv_reflect.h"

class ShaderReflector
{
public:
	ShaderReflector(const std::vector<char>& sourceCode);
	~ShaderReflector();

	VkDescriptorSetLayoutBinding GetDescriptorSetLayoutBinding(uint32_t bindingIndex, uint32_t setIndex = 0) const; // the set is by default 0 in GLSL

private:
	SpvReflectShaderModule module{};
};

class ShaderGroupReflector
{
public:
	ShaderGroupReflector(const std::vector<std::vector<char>>& sourceCodes);
	~ShaderGroupReflector();

	void ExcludeBinding(uint32_t set, uint32_t binding);

	std::vector<VkDescriptorSetLayoutBinding> GetLayoutBindingsOfSet(uint32_t setIndex) const;
	std::vector<VkDescriptorPoolSize> GetDescriptorPoolSize() const;
	uint32_t GetDescriptorSetCount() const;

	void WriteToDescriptorSet(VkDevice logicalDevice, VkDescriptorSet set, VkBuffer buffer, uint32_t setIndex, uint32_t binding) const;

private:
	std::set<uint64_t> removedBindings;
	std::vector<SpvReflectShaderModule> modules;
};