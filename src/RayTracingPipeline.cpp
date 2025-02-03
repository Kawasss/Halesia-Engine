#include "renderer/RayTracingPipeline.h"
#include "renderer/ShaderReflector.h"
#include "renderer/FramesInFlight.h"
#include "renderer/Vulkan.h"

#include "io/IO.h"

RayTracingPipeline::RayTracingPipeline(const std::string& rgen, const std::string& rchit, const std::string& rmiss)
{
	std::vector<std::vector<char>> shaders =
	{
		IO::ReadFile(rgen),
		IO::ReadFile(rchit),
		IO::ReadFile(rmiss),
	};
	ShaderGroupReflector reflector(shaders);

	InitializeBase(reflector);
	CreatePipeline(shaders);
}

void RayTracingPipeline::CreatePipeline(const std::vector<std::vector<char>>& shaders)
{
	// shaders[0] = rgen, [1] = rchit, [2] = rmiss
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkShaderModule genShader = Vulkan::CreateShaderModule(shaders[0]);
	VkShaderModule hitShader = Vulkan::CreateShaderModule(shaders[1]);
	VkShaderModule missShader = Vulkan::CreateShaderModule(shaders[2]);
	VkShaderModule shadowShader = Vulkan::CreateShaderModule(shaders[3]);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkResult result = vkCreatePipelineLayout(ctx.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &layout);
	CheckVulkanResult("Failed to create the pipeline layout for ray tracing", result, vkCreatePipelineLayout);

	std::array<VkPipelineShaderStageCreateInfo, 3> pipelineStageCreateInfos{};
	pipelineStageCreateInfos[0] = Vulkan::GetGenericShaderStageCreateInfo(genShader,  VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	pipelineStageCreateInfos[1] = Vulkan::GetGenericShaderStageCreateInfo(hitShader,  VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
	pipelineStageCreateInfos[2] = Vulkan::GetGenericShaderStageCreateInfo(missShader, VK_SHADER_STAGE_MISS_BIT_KHR);
	
	std::array<VkRayTracingShaderGroupCreateInfoKHR, 3> RTShaderGroupCreateInfos{};
	for (uint32_t i = 0; i < RTShaderGroupCreateInfos.size(); i++)
	{
		RTShaderGroupCreateInfos[i].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		RTShaderGroupCreateInfos[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		RTShaderGroupCreateInfos[i].closestHitShader = VK_SHADER_UNUSED_KHR;
		RTShaderGroupCreateInfos[i].anyHitShader = VK_SHADER_UNUSED_KHR;
		RTShaderGroupCreateInfos[i].intersectionShader = VK_SHADER_UNUSED_KHR;
		RTShaderGroupCreateInfos[i].generalShader = VK_SHADER_UNUSED_KHR;
	}
	RTShaderGroupCreateInfos[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR; // rgen
	RTShaderGroupCreateInfos[1].generalShader = 0;

	RTShaderGroupCreateInfos[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR; // rchit
	RTShaderGroupCreateInfos[1].closestHitShader = 1;

	RTShaderGroupCreateInfos[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR; // rmiss
	RTShaderGroupCreateInfos[1].generalShader = 2;

	VkRayTracingPipelineCreateInfoKHR RTPipelineCreateInfo{};
	RTPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	RTPipelineCreateInfo.stageCount = static_cast<uint32_t>(pipelineStageCreateInfos.size());
	RTPipelineCreateInfo.pStages = pipelineStageCreateInfos.data();
	RTPipelineCreateInfo.groupCount = static_cast<uint32_t>(RTShaderGroupCreateInfos.size());
	RTPipelineCreateInfo.pGroups = RTShaderGroupCreateInfos.data();
	RTPipelineCreateInfo.maxPipelineRayRecursionDepth = 1;
	RTPipelineCreateInfo.layout = layout;
	RTPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	RTPipelineCreateInfo.basePipelineIndex = 0;

	result = vkCreateRayTracingPipelinesKHR(ctx.logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &RTPipelineCreateInfo, nullptr, &pipeline);
	CheckVulkanResult("Failed to create the pipeline for ray tracing", result, vkCreateRayTracingPipelinesKHR);

	vkDestroyShaderModule(ctx.logicalDevice, shadowShader, nullptr);
	vkDestroyShaderModule(ctx.logicalDevice, missShader, nullptr);
	vkDestroyShaderModule(ctx.logicalDevice, hitShader, nullptr);
	vkDestroyShaderModule(ctx.logicalDevice, genShader, nullptr);
}