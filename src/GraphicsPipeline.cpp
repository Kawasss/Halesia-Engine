#include "renderer/Vulkan.h"

#include "renderer/GraphicsPipeline.h"
#include "renderer/ShaderReflector.h"

#include "tools/common.h"

GraphicsPipeline::GraphicsPipeline(const std::string& vertPath, const std::string& fragPath, PipelineOptions flags, VkRenderPass renderPass)
{
	//std::vector<char> vertCode = ReadFile(vertPath), fragCode = ReadFile(fragPath);
	std::vector<std::vector<char>> shaderCodes =
	{
		ReadFile(vertPath), // vert is [0]
		ReadFile(fragPath), // frag is [1]
	};
	ShaderGroupReflector reflector(shaderCodes);

	CreateDescriptorPool(reflector);
	CreateSetLayout(reflector);
	AllocateDescriptorSets(reflector.GetDescriptorSetCount());

	CreatePipelineLayout(reflector);
	CreateGraphicsPipeline(shaderCodes, flags, renderPass);
}

GraphicsPipeline::~GraphicsPipeline()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	vkDestroyPipeline(ctx.logicalDevice, pipeline, nullptr);
	vkDestroyPipelineLayout(ctx.logicalDevice, layout, nullptr);

	for (const VkDescriptorSetLayout& setLayout : setLayouts)
		vkDestroyDescriptorSetLayout(ctx.logicalDevice, setLayout, nullptr);

	vkDestroyDescriptorPool(ctx.logicalDevice, descriptorPool, nullptr);
}

void GraphicsPipeline::Bind(VkCommandBuffer commandBuffer)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);
}

void GraphicsPipeline::CreateDescriptorPool(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	std::vector<VkDescriptorPoolSize> poolSizes = reflector.GetDescriptorPoolSize();

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.maxSets = reflector.GetDescriptorSetCount();
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes = poolSizes.data();

	VkResult result = vkCreateDescriptorPool(ctx.logicalDevice, &createInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create the descriptor pool for a graphics pipeline", result, vkCreateDescriptorPool);
}

void GraphicsPipeline::CreateSetLayout(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	std::vector<uint32_t> indices = reflector.GetDescriptorSetIndices();
	setLayouts.resize(indices.size());

	for (int i = 0; i < indices.size(); i++) 
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = reflector.GetLayoutBindingsOfSet(indices[i]); // add support for multiple sets

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		createInfo.pBindings = bindings.data();

		VkResult result = vkCreateDescriptorSetLayout(ctx.logicalDevice, &createInfo, nullptr, &setLayouts[i]);
		CheckVulkanResult("Failed to create a set layout for a graphics pipeline", result, vkCreateDescriptorSetLayout);
	}
}

void GraphicsPipeline::AllocateDescriptorSets(uint32_t amount)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = amount;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.pSetLayouts = setLayouts.data();

	descriptorSets.resize(amount);
	VkResult result = vkAllocateDescriptorSets(ctx.logicalDevice, &allocInfo, descriptorSets.data());
	CheckVulkanResult("Failed to allocate descriptor sets for a graphics pipeline", result, vkAllocateDescriptorSets);
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

void GraphicsPipeline::CreateGraphicsPipeline(const std::vector<std::vector<char>>& shaders, PipelineOptions flags, VkRenderPass renderPass)
{
	VkShaderModule vertModule = Vulkan::CreateShaderModule(shaders[0]);
	VkShaderModule fragModule = Vulkan::CreateShaderModule(shaders[1]);

	std::vector<VkPipelineShaderStageCreateInfo> shaderInfos = 
	{
		Vulkan::GetGenericShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT),
		Vulkan::GetGenericShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	pipeline = PipelineCreator::CreatePipeline(layout, renderPass, shaderInfos, flags);

	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkDestroyShaderModule(ctx.logicalDevice, vertModule, nullptr);
	vkDestroyShaderModule(ctx.logicalDevice, fragModule, nullptr);
}