export module Renderer.ShaderReflector;

import <vulkan/vulkan.h>;
import <spirv-reflect/spirv_reflect.h>;

import std;

export class ShaderGroupReflector
{
public:
	union Binding
	{
		struct
		{
			std::uint32_t set;
			std::uint32_t binding;
		};
		std::uint64_t full = 0;

		Binding() = default;
		Binding(std::uint32_t set, std::uint32_t binding) : set(set), binding(binding) {}

		bool operator==(const Binding& other) const { return full == other.full; }
		bool operator< (const Binding& other) const { return full < other.full; }
	};

	ShaderGroupReflector(const std::span<char>& sourceCode);
	ShaderGroupReflector(const std::span<std::vector<char>>& sourceCodes);
	ShaderGroupReflector(const std::span<std::span<char>>& sourceCodes);
	~ShaderGroupReflector();

	void ExcludeSet(std::uint32_t set);

	std::vector<VkDescriptorSetLayoutBinding> GetLayoutBindingsOfSet(uint32_t setIndex) const;
	std::vector<VkDescriptorPoolSize>         GetDescriptorPoolSize() const;
	std::vector<VkPushConstantRange>          GetPushConstants() const;

	std::set<std::uint32_t> GetDescriptorSetIndices() const;

	std::uint32_t GetDescriptorSetCount() const;
	std::uint32_t GetOutputVariableCount(std::uint32_t index) const; // gets the amount of output variables of sourceCodes[index]

	const char* GetNameOfBinding(const Binding& binding) const;

	void WriteToDescriptorSet(VkDevice logicalDevice, VkDescriptorSet set, VkBuffer buffer, uint32_t setIndex, uint32_t binding) const;

private:
	void ProcessLayoutBindings();

	std::set<std::uint32_t> removedSets;
	std::vector<SpvReflectShaderModule> modules;
	std::map<std::uint32_t, std::vector<VkDescriptorSetLayoutBinding>> setLayoutBindings;
};