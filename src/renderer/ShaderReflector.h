#pragma once
#include <set>
#include <vector>
#include <vulkan/vulkan.h>
#include <unordered_map>
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
	union Binding
	{
		struct
		{
			uint32_t set;
			uint32_t binding;
		};
		uint64_t full = 0;

		Binding() = default;
		Binding(uint32_t set, uint32_t binding) : set(set), binding(binding) {}

		bool operator==(const Binding& other) const { return full == other.full; }
		bool operator< (const Binding& other) const { return full <  other.full; }
	};

	ShaderGroupReflector(const std::vector<std::vector<char>>& sourceCodes);
	~ShaderGroupReflector();

	void ExcludeBinding(uint32_t set, uint32_t binding);

	std::vector<VkDescriptorSetLayoutBinding> GetLayoutBindingsOfSet(uint32_t setIndex) const;
	std::vector<VkDescriptorPoolSize> GetDescriptorPoolSize() const;
	std::vector<VkPushConstantRange> GetPushConstants() const;
	std::set<uint32_t> GetDescriptorSetIndices() const;
	uint32_t GetDescriptorSetCount() const;

	const char* GetNameOfBinding(const Binding& binding) const;
	SpvReflectDescriptorBinding GetDescriptorBindingFromBinding(const Binding& binding) const;

	void WriteToDescriptorSet(VkDevice logicalDevice, VkDescriptorSet set, VkBuffer buffer, uint32_t setIndex, uint32_t binding) const;

private:
	void ProcessLayoutBindings();

	std::set<Binding> removedBindings;
	std::vector<SpvReflectShaderModule> modules;
	std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> setLayoutBindings;
};

template<>
struct std::hash<ShaderGroupReflector::Binding>
{
	std::size_t operator()(const ShaderGroupReflector::Binding val) const
	{
		return val.full;
	}
};