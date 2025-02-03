#include <algorithm>

#include "renderer/Vulkan.h"
#include "renderer/ShaderReflector.h"

ShaderGroupReflector::ShaderGroupReflector(const std::vector<char>& sourceCode)
{
	modules.resize(1);
	SpvReflectResult result = spvReflectCreateShaderModule(sourceCode.size(), sourceCode.data(), &modules[0]);
	if (result != SPV_REFLECT_RESULT_SUCCESS)
		throw VulkanAPIError("Cannot reflect on a given shader (code: " + std::to_string((int)result) + ')', VK_SUCCESS, "spvReflectCreateShaderModule", __FILENAME__, __LINE__);
	ProcessLayoutBindings();
}

ShaderGroupReflector::ShaderGroupReflector(const std::vector<std::vector<char>>& sourceCodes)
{
	modules.resize(sourceCodes.size());
	for (int i = 0; i < sourceCodes.size(); i++)
	{
		SpvReflectResult result = spvReflectCreateShaderModule(sourceCodes[i].size(), sourceCodes[i].data(), &modules[i]);
		if (result  != SPV_REFLECT_RESULT_SUCCESS)
			throw VulkanAPIError("Cannot reflect on a given shader (code: " + std::to_string((int)result) + ')', VK_SUCCESS, "spvReflectCreateShaderModule", __FILENAME__, __LINE__);
	}
	ProcessLayoutBindings();
}

static uint64_t CreateUniqueID(uint32_t set, uint32_t binding)
{
	return ShaderGroupReflector::Binding(set, binding).full;
}

void ShaderGroupReflector::ProcessLayoutBindings()
{
	for (int i = 0; i < modules.size(); i++)
	{
		for (uint32_t j = 0; j < modules[i].descriptor_binding_count; j++)
		{
			SpvReflectDescriptorBinding& current = modules[i].descriptor_bindings[j];
			Binding bindingID = { current.set, current.binding };

			if (setLayoutBindings.find(bindingID.set) == setLayoutBindings.end())
				setLayoutBindings[bindingID.set] = {};
			std::vector<VkDescriptorSetLayoutBinding>& layoutBinding = setLayoutBindings[bindingID.set];

			auto it = std::find_if(layoutBinding.begin(), layoutBinding.end(), [&](const VkDescriptorSetLayoutBinding& bind) { return bind.binding == bindingID.binding; }); // do not add duplicates
			if (it != layoutBinding.end())
			{
				it->stageFlags |= modules[i].shader_stage;
				continue;
			}
				
			VkDescriptorSetLayoutBinding binding{};
			binding.descriptorType = (VkDescriptorType)current.descriptor_type;
			binding.stageFlags = modules[i].shader_stage;
			binding.binding = current.binding;
			binding.pImmutableSamplers = nullptr;
			binding.descriptorCount = current.count;

			layoutBinding.push_back(binding);
		}
	}
}

std::vector<VkDescriptorSetLayoutBinding> ShaderGroupReflector::GetLayoutBindingsOfSet(uint32_t setIndex) const
{
	return setLayoutBindings.find(setIndex)->second;
}

std::vector<VkDescriptorPoolSize> ShaderGroupReflector::GetDescriptorPoolSize() const
{
	std::vector<VkDescriptorPoolSize> ret;
	std::map<VkDescriptorType, size_t> typeIndex; // where in 'ret' is this descriptor type

	for (const auto& [set, bindings] : setLayoutBindings)
	{
		for (const VkDescriptorSetLayoutBinding& binding : bindings)
		{
			auto it = typeIndex.find(binding.descriptorType);
			if (it == typeIndex.end()) // if this descriptor isnt registered, then register it. otherwise, add the amount of newly found descriptors onto it
			{
				VkDescriptorPoolSize size{};
				size.descriptorCount = binding.descriptorCount;
				size.type = binding.descriptorType;

				ret.push_back(size);
				typeIndex.emplace(binding.descriptorType, ret.size() - 1);
			}
			else
				ret[it->second].descriptorCount += binding.descriptorCount;
		}
	}
	return ret;
}

std::vector<VkPushConstantRange> ShaderGroupReflector::GetPushConstants() const
{
	std::vector<VkPushConstantRange> ret;
	for (int i = 0; i < modules.size(); i++)
	{
		for (uint32_t j = 0; j < modules[i].push_constant_block_count; j++)
		{
			if (ret.empty())
			{
				VkPushConstantRange range{};
				range.offset = modules[i].push_constant_blocks[j].offset;
				range.size = modules[i].push_constant_blocks[j].size;
				range.stageFlags = modules[i].shader_stage;

				ret.push_back(range);
			}
			else
			{
				ret[0].stageFlags |= modules[i].shader_stage;
			}
		}
	}
	return ret;
}

uint32_t ShaderGroupReflector::GetDescriptorSetCount() const
{
	return static_cast<uint32_t>(setLayoutBindings.size());
}

uint32_t ShaderGroupReflector::GetOutputVariableCount(uint32_t index) const
{
	return modules[index].output_variable_count;
}

std::set<uint32_t> ShaderGroupReflector::GetDescriptorSetIndices() const
{
	std::set<uint32_t> ret;
	for (const auto& [index, bindings] : setLayoutBindings)
	{
		ret.insert(index);
	}
	return ret;
}

const char* ShaderGroupReflector::GetNameOfBinding(const Binding& binding) const
{
	for (uint32_t i = 0; i < modules.size(); i++)
	{
		for (uint32_t j = 0; j < modules[i].descriptor_binding_count; j++)
		{
			SpvReflectDescriptorBinding& descriptorBinding = modules[i].descriptor_bindings[j];
			Binding currBinding(descriptorBinding.set, descriptorBinding.binding);

			if (currBinding == binding)
				return descriptorBinding.name;
		}
	}
	return nullptr;
}

void ShaderGroupReflector::WriteToDescriptorSet(VkDevice logicalDevice, VkDescriptorSet set, VkBuffer buffer, uint32_t setIndex, uint32_t binding) const
{
	std::vector<VkDescriptorSetLayoutBinding> bindings = GetLayoutBindingsOfSet(setIndex); // not the fastest way, but cleaner
	VkDescriptorSetLayoutBinding descBinding{};
	for (int i = 0; i < bindings.size(); i++)
	{
		if (bindings[i].binding == binding)
			descBinding = bindings[i];
	}

	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = descBinding.descriptorType;
	writeSet.dstBinding = binding;
	writeSet.dstSet = set;
	writeSet.descriptorCount = 1;
	writeSet.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(logicalDevice, 1, &writeSet, 0, nullptr);
}

void ShaderGroupReflector::ExcludeBinding(uint32_t set, uint32_t binding)
{
	removedBindings.emplace(set, binding);
}

ShaderGroupReflector::~ShaderGroupReflector()
{
	for (int i = 0; i < modules.size(); i++)
		spvReflectDestroyShaderModule(&modules[i]);
}