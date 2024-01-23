#include <unordered_set>
#include <unordered_map>

#include "renderer/Vulkan.h"
#include "renderer/ShaderReflector.h"

ShaderReflector::ShaderReflector(const std::vector<char>& sourceCode)
{
	if (spvReflectCreateShaderModule(sourceCode.size(), sourceCode.data(), &module) != SPV_REFLECT_RESULT_SUCCESS)
		throw VulkanAPIError("Cannot reflect on a given shader", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
}

VkDescriptorSetLayoutBinding ShaderReflector::GetDescriptorSetLayoutBinding(uint32_t bindingIndex, uint32_t setIndex)
{
	if (setIndex >= module.descriptor_set_count)
		throw VulkanAPIError("Cannot get the set bindings from a given shader", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
	

	for (int i = 0; i < module.descriptor_sets[setIndex].binding_count; i++)
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
		if (spvReflectCreateShaderModule(sourceCodes[i].size(), sourceCodes[i].data(), &modules[i]) != SPV_REFLECT_RESULT_SUCCESS)
			throw VulkanAPIError("Cannot reflect on a given shader", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
	}
}

std::vector<VkDescriptorSetLayoutBinding> ShaderGroupReflector::GetLayoutBindingsOfSet(uint32_t setIndex)
{
	std::vector<VkDescriptorSetLayoutBinding> ret;
	std::unordered_map<uint64_t, int> uniqueBindingIndexMap;
	for (uint32_t i = 0; i < modules.size(); i++)
	{
		for (uint32_t j = 0; j < modules[i].descriptor_binding_count; j++)
		{
			SpvReflectDescriptorBinding& current = modules[i].descriptor_bindings[j];
			uint64_t bindingID = 0; // encode 2 uint32_t's into one uint64_t
			memcpy(&bindingID, &current.set, sizeof(current.set));
			memcpy((unsigned char*)&bindingID + 4, &current.binding, sizeof(current.binding));

			if (uniqueBindingIndexMap.count(bindingID) > 0) // if that binding already exists, then dont create a new one
			{
				ret[uniqueBindingIndexMap[bindingID]].stageFlags |= modules[i].shader_stage; // not a good way if determining the index
				continue;
			}
			if (current.set != setIndex)
				continue;

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

ShaderGroupReflector::~ShaderGroupReflector()
{
	for (int i = 0; i < modules.size(); i++)
		spvReflectDestroyShaderModule(&modules[i]);
}