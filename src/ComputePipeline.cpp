module;

#include "renderer/Vulkan.h"
#include "renderer/CommandBuffer.h"
#include "renderer/VulkanAPIError.h"

module Renderer.ComputePipeline;

import std;

import Renderer.CompiledShader;
import Renderer.ShaderCompiler;
import Renderer.ShaderReflector;
import Renderer.DescriptorWriter;

ComputePipeline::ComputePipeline(const std::string_view& path)
{
	std::expected<CompiledShader, bool> shader = ShaderCompiler::Compile(path);
	if (!shader.has_value())
		return;

	ShaderGroupReflector reflector(shader->code);
	for (uint32_t set : shader->externalSets)
		reflector.ExcludeSet(set);

	VkShaderModule module = Vulkan::CreateShaderModule(shader->code);

	InitializeBase(reflector);

	CreatePipelineLayout(reflector);
	CreateComputePipeline(module);

	bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
}

void ComputePipeline::CreatePipelineLayout(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& context = Vulkan::GetContext();
	std::vector<VkPushConstantRange> ranges = reflector.GetPushConstants();

	VkPipelineLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	createInfo.pushConstantRangeCount = static_cast<std::uint32_t>(ranges.size());
	createInfo.pPushConstantRanges = ranges.data();
	createInfo.pSetLayouts = setLayouts.data();
	createInfo.setLayoutCount = static_cast<std::uint32_t>(setLayouts.size());

	VkResult result = vkCreatePipelineLayout(context.logicalDevice, &createInfo, nullptr, &layout);
	CheckVulkanResult("Failed to create a pipeline layout", result);
}

void ComputePipeline::CreateComputePipeline(VkShaderModule module)
{
	const Vulkan::Context& context = Vulkan::GetContext();

	VkPipelineShaderStageCreateInfo stageCreateInfo{};
	stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageCreateInfo.module = module;
	stageCreateInfo.pName = "main";

	VkComputePipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	createInfo.layout = layout;
	createInfo.stage = stageCreateInfo;

	VkResult result = vkCreateComputePipelines(context.logicalDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
	CheckVulkanResult("Failed to create a compute pipeline", result);

	vkDestroyShaderModule(context.logicalDevice, module, nullptr);
}

void ComputePipeline::Execute(const CommandBuffer& commandBuffer, std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
	Bind(commandBuffer);
	commandBuffer.Dispatch(x, y, z);
}