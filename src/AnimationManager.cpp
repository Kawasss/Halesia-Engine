#include <vector>

#include "renderer/AnimationManager.h"
#include "renderer/ShaderReflector.h"
#include "renderer/Vulkan.h"

#include "tools/common.h"

AnimationManager* AnimationManager::Get()
{
	static AnimationManager* singleton = nullptr;
	if (singleton == nullptr)
	{
		singleton = new AnimationManager();
		singleton->Create();
	}
	return singleton;
}

void AnimationManager::Create()
{
	CreateShader();
}

void AnimationManager::Compute(VkCommandBuffer commandBuffer)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdDispatch(commandBuffer, 1, 0, 0);
}

void AnimationManager::CreateShader()
{
	const Vulkan::Context& context = Vulkan::GetContext();
	logicalDevice = context.logicalDevice;
	computeQueue = context.computeQueue;

	std::vector<char> code = ReadFile("shaders/spirv/anim.comp.spv");
	ShaderGroupReflector reflector({ code });

	// descriptor set

	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = reflector.GetDescriptorPoolSize();

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	poolCreateInfo.maxSets = 1;

	VkResult result = vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create a descriptor pool", result, vkCreateDescriptorPool);

	// set layout and pipeline

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = reflector.GetLayoutBindingsOfSet(0);

	std::vector<VkDescriptorBindingFlags> setBindingFlags(setLayoutBindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setBindingFlagsCreateInfo{};
	setBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	setBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(setBindingFlags.size());
	setBindingFlagsCreateInfo.pBindingFlags = setBindingFlags.data();

	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
	setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
	setLayoutCreateInfo.pBindings = setLayoutBindings.data();
	setLayoutCreateInfo.pNext = &setBindingFlagsCreateInfo;

	result = vkCreateDescriptorSetLayout(logicalDevice, &setLayoutCreateInfo, nullptr, &computeSetLayout);
	CheckVulkanResult("Failed to create the descriptor set layout", result, vkCreateDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pSetLayouts = &computeSetLayout;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = 1;

	result = vkAllocateDescriptorSets(logicalDevice, &allocateInfo, &descriptorSet);
	CheckVulkanResult("Failed to allocate a descriptor set", result, vkAllocateDescriptorSets);
	
	VkPipelineLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = &computeSetLayout;
	layoutCreateInfo.setLayoutCount = 1;

	result = vkCreatePipelineLayout(logicalDevice, &layoutCreateInfo, nullptr, &computeLayout);
	CheckVulkanResult("Failed to create a pipeline layout", result, vkCreatePipelineLayout);

	VkShaderModule computeModule = Vulkan::CreateShaderModule(code);

	VkPipelineShaderStageCreateInfo stageCreateInfo{};
	stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageCreateInfo.module = computeModule;
	stageCreateInfo.pName = "main";

	VkComputePipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	createInfo.layout = computeLayout;
	createInfo.stage = stageCreateInfo;

	result = vkCreateComputePipelines(logicalDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &computePipeline);

	vkDestroyShaderModule(logicalDevice, computeModule, nullptr);
}

void AnimationManager::Destroy()
{
	vkDestroyPipelineLayout(logicalDevice, computeLayout, nullptr);
	vkDestroyPipeline(logicalDevice, computePipeline, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, computeSetLayout, nullptr);
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
}