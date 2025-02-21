#include "renderer/Vulkan.h"
#include "renderer/ComputeShader.h"
#include "renderer/ShaderReflector.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/GarbageManager.h"
#include "renderer/CommandBuffer.h"
#include "renderer/VulkanAPIError.h"

#include "io/IO.h"

ComputeShader::ComputeShader(const std::string& path)
{
	std::vector<char> code = IO::ReadFile(path);
	ShaderGroupReflector reflector(code);
	VkShaderModule module = Vulkan::CreateShaderModule(code);

	InitializeBase(reflector);

	CreatePipelineLayout();
	CreateComputePipeline(module);

	bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
}

void ComputeShader::CreatePipelineLayout()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	VkPipelineLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pSetLayouts = setLayouts.data();
	layoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());

	VkResult result = vkCreatePipelineLayout(context.logicalDevice, &layoutCreateInfo, nullptr, &layout);
	CheckVulkanResult("Failed to create a pipeline layout", result);
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
	createInfo.layout = layout;
	createInfo.stage = stageCreateInfo;

	VkResult result = vkCreateComputePipelines(context.logicalDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
	CheckVulkanResult("Failed to create a compute pipeline", result);

	vkDestroyShaderModule(context.logicalDevice, module, nullptr);
}

void ComputeShader::Execute(const CommandBuffer& commandBuffer, uint32_t x, uint32_t y, uint32_t z)
{
	Bind(commandBuffer);
	commandBuffer.Dispatch(x, y, z);
}