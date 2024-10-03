#include "renderer/Vulkan.h"
#include "renderer/ComputeShader.h"
#include "renderer/ShaderReflector.h"
#include "renderer/DescriptorWriter.h"

#include "io/IO.h"

ComputeShader::ComputeShader(std::string path)
{
	std::vector<char> code = IO::ReadFile(path);
	ShaderGroupReflector reflector({ code });
	VkShaderModule module = Vulkan::CreateShaderModule(code);

	CreateDescriptorPool(reflector);
	CreateSetLayout(reflector);
	AllocateDescriptorSets(reflector.GetDescriptorSetCount());
	CreatePipelineLayout();
	CreateComputePipeline(module);
}

ComputeShader::~ComputeShader()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	vkDestroyPipelineLayout(context.logicalDevice, pipelineLayout, nullptr);
	vkDestroyPipeline(context.logicalDevice, pipeline, nullptr);

	for (const VkDescriptorSetLayout& setLayout : setLayouts)
		vkDestroyDescriptorSetLayout(context.logicalDevice, setLayout, nullptr);
	vkDestroyDescriptorPool(context.logicalDevice, descriptorPool, nullptr);
}

void ComputeShader::CreateDescriptorPool(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& context = Vulkan::GetContext();
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = reflector.GetDescriptorPoolSize();

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	poolCreateInfo.maxSets = reflector.GetDescriptorSetCount();

	VkResult result = vkCreateDescriptorPool(context.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create a descriptor pool", result, vkCreateDescriptorPool);
}

void ComputeShader::CreateSetLayout(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& context = Vulkan::GetContext();
	std::set<uint32_t> indices = reflector.GetDescriptorSetIndices();

	setLayouts.reserve(indices.size());

	for (uint32_t index : indices)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = reflector.GetLayoutBindingsOfSet(index);

		VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
		setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		setLayoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		setLayoutCreateInfo.pBindings = bindings.data();

		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		VkResult result = vkCreateDescriptorSetLayout(context.logicalDevice, &setLayoutCreateInfo, nullptr, &setLayout);
		CheckVulkanResult("Failed to create the descriptor set layout", result, vkCreateDescriptorSetLayout);

		setLayouts.push_back(setLayout);

		for (const VkDescriptorSetLayoutBinding& binding : bindings)
		{
			ShaderGroupReflector::Binding bind(index, binding.binding);
			BindingLayout& bindingLayout = nameToLayout[reflector.GetNameOfBinding(bind)];
			bindingLayout.set = index;
			bindingLayout.binding = binding;
		}
	}
}

void ComputeShader::AllocateDescriptorSets(uint32_t amount)
{
	const Vulkan::Context& context = Vulkan::GetContext();

	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pSetLayouts = setLayouts.data();
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = amount;

	sets.resize(amount);
	VkResult result = vkAllocateDescriptorSets(context.logicalDevice, &allocateInfo, sets.data());
	CheckVulkanResult("Failed to allocate a descriptor set", result, vkAllocateDescriptorSets);
}

void ComputeShader::CreatePipelineLayout()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	VkPipelineLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = setLayouts.data();
	layoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());

	VkResult result = vkCreatePipelineLayout(context.logicalDevice, &layoutCreateInfo, nullptr, &pipelineLayout);
	CheckVulkanResult("Failed to create a pipeline layout", result, vkCreatePipelineLayout);
}

void ComputeShader::CreateComputePipeline(VkShaderModule module)
{
	const Vulkan::Context& context = Vulkan::GetContext();

	VkPipelineShaderStageCreateInfo stageCreateInfo{};
	stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageCreateInfo.module = module;
	stageCreateInfo.pName = "main";

	VkComputePipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	createInfo.layout = pipelineLayout;
	createInfo.stage = stageCreateInfo;

	VkResult result = vkCreateComputePipelines(context.logicalDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
	CheckVulkanResult("Failed to create a compute pipeline", result, vkCreatePipelineLayout);

	vkDestroyShaderModule(context.logicalDevice, module, nullptr);
}

void ComputeShader::Execute(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, (uint32_t)sets.size(), sets.data(), 0, nullptr);
	vkCmdDispatch(commandBuffer, x, y, z);
}

void ComputeShader::BindBufferToName(const std::string& name, VkBuffer buffer)
{
	const BindingLayout& binding = nameToLayout[name];

	DescriptorWriter* writer = DescriptorWriter::Get();
	writer->WriteBuffer(sets[binding.set], buffer, binding.binding.descriptorType, binding.binding.binding);
}

void ComputeShader::BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout)
{
	const BindingLayout& binding = nameToLayout[name];

	DescriptorWriter* writer = DescriptorWriter::Get();
	writer->WriteImage(sets[binding.set], binding.binding.descriptorType, binding.binding.binding, view, sampler, layout);
}