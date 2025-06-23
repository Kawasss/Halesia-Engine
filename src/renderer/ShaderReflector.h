#pragma once
#include <set>
#include <vector>
#include <map>
#include <span>

#include <vulkan/vulkan.h>
#include <spirv-reflect/spirv_reflect.h>

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

	ShaderGroupReflector(const std::span<char>& sourceCode);
	ShaderGroupReflector(const std::span<std::vector<char>>& sourceCodes);
	ShaderGroupReflector(const std::span<std::span<char>>& sourceCodes);
	~ShaderGroupReflector();

	void ExcludeSet(uint32_t set);

	std::vector<VkDescriptorSetLayoutBinding> GetLayoutBindingsOfSet(uint32_t setIndex) const;
	std::vector<VkDescriptorPoolSize>         GetDescriptorPoolSize() const;
	std::vector<VkPushConstantRange>          GetPushConstants() const;
	
	std::set<uint32_t> GetDescriptorSetIndices() const;

	uint32_t GetDescriptorSetCount() const;
	uint32_t GetOutputVariableCount(uint32_t index) const; // gets the amount of output variables of sourceCodes[index]

	const char* GetNameOfBinding(const Binding& binding) const;

	void WriteToDescriptorSet(VkDevice logicalDevice, VkDescriptorSet set, VkBuffer buffer, uint32_t setIndex, uint32_t binding) const;

private:
	void ProcessLayoutBindings();

	std::set<uint32_t> removedSets;
	std::vector<SpvReflectShaderModule> modules;
	std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> setLayoutBindings;
};