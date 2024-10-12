#include <map>

#include "renderer/Vulkan.h"

#include "renderer/GraphicsPipeline.h"
#include "renderer/ShaderReflector.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/Buffer.h"

#include "io/IO.h"

GraphicsPipeline::GraphicsPipeline(const std::string& vertPath, const std::string& fragPath, PipelineOptions flags, VkRenderPass renderPass)
{
	//std::vector<char> vertCode = ReadFile(vertPath), fragCode = ReadFile(fragPath);
	std::vector<std::vector<char>> shaderCodes =
	{
		IO::ReadFile(vertPath), // vert is [0]
		IO::ReadFile(fragPath), // frag is [1]
	};
	ShaderGroupReflector reflector(shaderCodes);

	CreateDescriptorPool(reflector);
	CreateSetLayout(reflector);
	AllocateDescriptorSets(reflector.GetDescriptorSetCount());

	CreatePipelineLayout(reflector);
	CreateGraphicsPipeline(shaderCodes, flags, renderPass, reflector.GetOutputVariableCount(1));
}

GraphicsPipeline::~GraphicsPipeline()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	vkDestroyPipeline(ctx.logicalDevice, pipeline, nullptr);
	vkDestroyPipelineLayout(ctx.logicalDevice, layout, nullptr);

	for (const VkDescriptorSetLayout& setLayout : setLayouts)
		vkDestroyDescriptorSetLayout(ctx.logicalDevice, setLayout, nullptr);

	vkDestroyDescriptorPool(ctx.logicalDevice, pool, nullptr);
}

void GraphicsPipeline::Bind(VkCommandBuffer commandBuffer)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, static_cast<uint32_t>(descriptorSets[FIF::frameIndex].size()), descriptorSets[FIF::frameIndex].data(), 0, nullptr);
}

void GraphicsPipeline::CreateDescriptorPool(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	std::vector<VkDescriptorPoolSize> poolSizes = reflector.GetDescriptorPoolSize();

	for (VkDescriptorPoolSize& size : poolSizes)
		size.descriptorCount *= FIF::FRAME_COUNT;

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.maxSets = reflector.GetDescriptorSetCount() * FIF::FRAME_COUNT;
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes = poolSizes.data();

	VkResult result = vkCreateDescriptorPool(ctx.logicalDevice, &createInfo, nullptr, &pool);
	CheckVulkanResult("Failed to create the descriptor pool for a graphics pipeline", result, vkCreateDescriptorPool);
}

void GraphicsPipeline::CreateSetLayout(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	std::set<uint32_t> indices = reflector.GetDescriptorSetIndices();

	setLayouts.reserve(indices.size());

	for (uint32_t index : indices) 
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = reflector.GetLayoutBindingsOfSet(index);

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		createInfo.pBindings = bindings.data();

		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		VkResult result = vkCreateDescriptorSetLayout(ctx.logicalDevice, &createInfo, nullptr, &setLayout);
		CheckVulkanResult("Failed to create a set layout for a graphics pipeline", result, vkCreateDescriptorSetLayout);

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

void GraphicsPipeline::AllocateDescriptorSets(uint32_t amount)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = amount;
	allocInfo.descriptorPool = pool;
	allocInfo.pSetLayouts = setLayouts.data();

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
	{
		descriptorSets[i].resize(amount);
		VkResult result = vkAllocateDescriptorSets(ctx.logicalDevice, &allocInfo, descriptorSets[i].data());
		CheckVulkanResult("Failed to allocate descriptor sets for a graphics pipeline", result, vkAllocateDescriptorSets);
	}
}

void GraphicsPipeline::CreatePipelineLayout(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	std::vector<VkPushConstantRange> ranges = reflector.GetPushConstants();

	VkPipelineLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	createInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
	createInfo.pPushConstantRanges = ranges.data();
	createInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	createInfo.pSetLayouts = setLayouts.data();

	VkResult result = vkCreatePipelineLayout(ctx.logicalDevice, &createInfo, nullptr, &layout);
	CheckVulkanResult("Failed to create a pipeline layout for a graphics pipeline", result, vkCreatePipelineLayout);
}

void GraphicsPipeline::CreateGraphicsPipeline(const std::vector<std::vector<char>>& shaders, PipelineOptions flags, VkRenderPass renderPass, uint32_t attachmentCount)
{
	VkShaderModule vertModule = Vulkan::CreateShaderModule(shaders[0]);
	VkShaderModule fragModule = Vulkan::CreateShaderModule(shaders[1]);

	std::vector<VkPipelineShaderStageCreateInfo> shaderInfos = 
	{
		Vulkan::GetGenericShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT),
		Vulkan::GetGenericShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	pipeline = PipelineCreator::CreatePipeline(layout, renderPass, shaderInfos, flags, attachmentCount);

	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkDestroyShaderModule(ctx.logicalDevice, vertModule, nullptr);
	vkDestroyShaderModule(ctx.logicalDevice, fragModule, nullptr);
}

void GraphicsPipeline::BindBufferToName(const std::string& name, VkBuffer buffer)
{
	const BindingLayout& binding = nameToLayout[name];
	
	DescriptorWriter* writer = DescriptorWriter::Get();
	
	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		writer->WriteBuffer(descriptorSets[i][binding.set], buffer, binding.binding.descriptorType, binding.binding.binding);
}

void GraphicsPipeline::BindBufferToName(const std::string& name, const FIF::Buffer& buffer)
{
	const BindingLayout& binding = nameToLayout[name];

	DescriptorWriter* writer = DescriptorWriter::Get();

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		writer->WriteBuffer(descriptorSets[i][binding.set], buffer[i], binding.binding.descriptorType, binding.binding.binding);
}

void GraphicsPipeline::BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout)
{
	const BindingLayout& binding = nameToLayout[name];

	DescriptorWriter* writer = DescriptorWriter::Get();

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		writer->WriteImage(descriptorSets[i][binding.set], binding.binding.descriptorType, binding.binding.binding, view, sampler, layout);
}

void GraphicsPipeline::BindImageToName(const std::string& name, uint32_t index, VkImageView view, VkSampler sampler, VkImageLayout layout)
{
	const BindingLayout& binding = nameToLayout[name];

	DescriptorWriter* writer = DescriptorWriter::Get();

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		writer->WriteImage(descriptorSets[i][binding.set], binding.binding.descriptorType, binding.binding.binding, view, sampler, layout, 1, index);
}

void GraphicsPipeline::PushConstant(VkCommandBuffer commandBuffer, const void* value, VkShaderStageFlags stages, uint32_t size, uint32_t offset)
{
	vkCmdPushConstants(commandBuffer, layout, stages, offset, size, value);
}