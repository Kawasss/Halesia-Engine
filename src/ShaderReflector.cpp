#include <unordered_map>
#include <algorithm>

#include "renderer/Vulkan.h"
#include "renderer/ShaderReflector.h"
#include "Vertex.h"
#include "glm.h"

std::unordered_map<SpvReflectFormat, uint32_t> sizeOfFormat{ { SPV_REFLECT_FORMAT_R32G32B32_SFLOAT, sizeof(glm::vec3) }, { SPV_REFLECT_FORMAT_R32G32_SFLOAT, sizeof(glm::vec2) }, { SPV_REFLECT_FORMAT_R32_SINT, sizeof(int) } };

ShaderReflector::ShaderReflector(const std::vector<char>& sourceCode)
{
	spvReflectCreateShaderModule(sourceCode.size(), sourceCode.data(), &module);
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
		ret.binding = binding->binding;
		ret.descriptorType = (VkDescriptorType)binding->descriptor_type;
		ret.stageFlags = (VkShaderStageFlags)module.shader_stage;
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