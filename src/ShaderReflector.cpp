#include <unordered_set>
#include <unordered_map>

#include "renderer/Vulkan.h"
#include "renderer/ShaderReflector.h"

ShaderReflector::ShaderReflector(const std::vector<char>& sourceCode)
{
	if (spvReflectCreateShaderModule(sourceCode.size(), sourceCode.data(), &module) != SPV_REFLECT_RESULT_SUCCESS)
		throw VulkanAPIError("Cannot reflect on a given shader", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
}

VkDescriptorSetLayoutBinding ShaderReflector::GetDescriptorSetLayoutBinding(uint32_t bindingIndex, uint32_t setIndex) const
{
	if (setIndex >= module.descriptor_set_count)
		throw VulkanAPIError("Cannot get the set bindings from a given shader", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
	

	for (uint32_t i = 0; i < module.descriptor_sets[setIndex].binding_count; i++)
	{
		if (module.descriptor_sets[setIndex].bindings[i]->binding != bindingIndex)
			continue;
		SpvReflectDescriptorBinding* binding = module.descriptor_sets[setIndex].bindings[i];

		VkDescriptorSetLayoutBinding ret{};
		ret.descriptorType = (VkDescriptorType)binding->descriptor_type;
		ret.stageFlags = (VkShaderStageFlags)module.shader_stage;
		ret.binding = binding->binding;
		ret.pImmutableSamplers = nullptr;
		ret.descriptorCount = 1; // descriptor count cannot be set with this, assume 1
		
		return ret;
	}
	throw VulkanAPIError("Cannot get the set bindings from a given shader", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
}

ShaderReflector::~ShaderReflector()
{
	spvReflectDestroyShaderModule(&module);
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
}

inline uint64_t CreateUniqueID(uint32_t set, uint32_t binding)
{
	uint64_t ret;
	memcpy(&ret, &set, sizeof(set));
	memcpy((unsigned char*)&ret + 4, &binding, sizeof(binding));
	return ret;
}

std::vector<VkDescriptorSetLayoutBinding> ShaderGroupReflector::GetLayoutBindingsOfSet(uint32_t setIndex) const
{
	std::vector<VkDescriptorSetLayoutBinding> ret;
	std::unordered_map<uint64_t, size_t> uniqueBindingIndexMap;
	for (uint32_t i = 0; i < modules.size(); i++)
	{
		for (uint32_t j = 0; j < modules[i].descriptor_binding_count; j++)
		{
			SpvReflectDescriptorBinding& current = modules[i].descriptor_bindings[j];
			if (current.set != setIndex)
				continue;

			uint64_t bindingID = CreateUniqueID(current.set, current.binding); // encode 2 uint32_t's into one uint64_t
			if (removedBindings.find(bindingID) != removedBindings.end())
				continue;

			if (uniqueBindingIndexMap.count(bindingID) > 0) // if that binding already exists, then dont create a new one
			{
				ret[uniqueBindingIndexMap[bindingID]].stageFlags |= modules[i].shader_stage; // not a good way if determining the index
				continue;
			}

			VkDescriptorSetLayoutBinding binding{};
			binding.descriptorType = (VkDescriptorType)current.descriptor_type;
			binding.stageFlags = modules[i].shader_stage;
			binding.binding = current.binding;
			binding.pImmutableSamplers = nullptr;
			binding.descriptorCount = 1;

			ret.push_back(binding);
			uniqueBindingIndexMap[bindingID] = ret.size() - 1;
		}
	}
	return ret;
}

std::vector<VkDescriptorPoolSize> ShaderGroupReflector::GetDescriptorPoolSize() const
{
	std::vector<VkDescriptorPoolSize> ret;
	std::unordered_map<SpvReflectDescriptorType, uint32_t> typeIndex;
	std::unordered_set<uint64_t> doesBindingAlreadyExist;

	for (uint32_t i = 0; i < modules.size(); i++)
	{
		for (uint32_t j = 0; j < modules[i].descriptor_binding_count; j++)
		{
			SpvReflectDescriptorBinding& current = modules[i].descriptor_bindings[j];
			uint64_t ID = CreateUniqueID(current.set, current.binding);
			
			if (doesBindingAlreadyExist.find(ID) == doesBindingAlreadyExist.end())
				doesBindingAlreadyExist.insert(ID);
			else continue;

			if (typeIndex.count(current.descriptor_type) == 0)
			{
				ret.push_back({ (VkDescriptorType)current.descriptor_type, 0 });
				typeIndex[current.descriptor_type] = static_cast<uint32_t>(ret.size() - 1);
			}
			ret[typeIndex[current.descriptor_type]].descriptorCount++;
		}
	}
	return ret;
}

std::vector<VkPushConstantRange> ShaderGroupReflector::GetPushConstants() const
{
	std::vector<VkPushConstantRange> ret;
	for (int i = 0; i < modules.size(); i++)
	{
		for (int j = 0; j < modules[i].push_constant_block_count; j++)
		{
			VkPushConstantRange range{};
			range.offset = modules[i].push_constant_blocks[j].offset;
			range.size = modules[i].push_constant_blocks[j].size;
			range.stageFlags = modules[i].shader_stage;
			
			ret.push_back(range);
		}
	}
	return ret;
}

uint32_t ShaderGroupReflector::GetDescriptorSetCount() const
{
	uint32_t ret = 0;
	for (int i = 0; i < modules.size(); i++)
		ret += modules[i].descriptor_set_count;
	return ret;
}

std::set<uint32_t> ShaderGroupReflector::GetDescriptorSetIndices() const
{
	std::set<uint32_t> ret;
	for (int i = 0; i < modules.size(); i++)
	{
		for (int j = 0; j < modules[i].descriptor_set_count; j++)
		{
			ret.insert(modules[i].descriptor_sets[j].set);
		}
	}
	return ret;
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
	removedBindings.insert(CreateUniqueID(set, binding));
}

ShaderGroupReflector::~ShaderGroupReflector()
{
	for (int i = 0; i < modules.size(); i++)
		spvReflectDestroyShaderModule(&modules[i]);
}