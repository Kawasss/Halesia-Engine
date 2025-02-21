#include "renderer/RayTracingPipeline.h"
#include "renderer/ShaderReflector.h"
#include "renderer/FramesInFlight.h"
#include "renderer/CommandBuffer.h"
#include "renderer/Vulkan.h"
#include "renderer/VulkanAPIError.h"

#include "io/IO.h"

constexpr uint32_t RT_GROUP_COUNT = 4;

VkStridedDeviceAddressRegionKHR fallbackShaderBinding{};

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
	CreatePipeline(reflector, shaders);
	CreateShaderBindingTable();

	bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
}

void RayTracingPipeline::CreatePipeline(const ShaderGroupReflector& reflector, const std::vector<std::vector<char>>& shaders)
{
	// shaders[0] = rgen, [1] = rchit, [2] = rmiss
	const Vulkan::Context& ctx = Vulkan::GetContext();
	std::vector<VkPushConstantRange> ranges = reflector.GetPushConstants();

	VkShaderModule genShader  = Vulkan::CreateShaderModule(shaders[0]);
	VkShaderModule hitShader  = Vulkan::CreateShaderModule(shaders[1]);
	VkShaderModule missShader = Vulkan::CreateShaderModule(shaders[2]);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
	pipelineLayoutCreateInfo.pPushConstantRanges = ranges.data();

	VkResult result = vkCreatePipelineLayout(ctx.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &layout);
	CheckVulkanResult("Failed to create the pipeline layout for ray tracing", result);

	std::array<VkPipelineShaderStageCreateInfo, RT_GROUP_COUNT> stageCreateInfos{};
	stageCreateInfos[0] = Vulkan::GetGenericShaderStageCreateInfo(hitShader,  VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
	stageCreateInfos[1] = Vulkan::GetGenericShaderStageCreateInfo(genShader,  VK_SHADER_STAGE_RAYGEN_BIT_KHR);
	stageCreateInfos[2] = Vulkan::GetGenericShaderStageCreateInfo(missShader, VK_SHADER_STAGE_MISS_BIT_KHR);
	stageCreateInfos[3] = Vulkan::GetGenericShaderStageCreateInfo(missShader, VK_SHADER_STAGE_MISS_BIT_KHR);

	std::array<VkRayTracingShaderGroupCreateInfoKHR, RT_GROUP_COUNT> shaderGroupCreateInfos{};
	shaderGroupCreateInfos[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shaderGroupCreateInfos[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	shaderGroupCreateInfos[0].generalShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCreateInfos[0].anyHitShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCreateInfos[0].intersectionShader = VK_SHADER_UNUSED_KHR;
	shaderGroupCreateInfos[0].closestHitShader = 0;

	for (uint32_t i = 1; i < shaderGroupCreateInfos.size(); i++)
	{
		shaderGroupCreateInfos[i].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroupCreateInfos[i].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroupCreateInfos[i].closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCreateInfos[i].anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCreateInfos[i].intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroupCreateInfos[i].generalShader = i;
	}

	VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(stageCreateInfos.size());
	pipelineCreateInfo.pStages = stageCreateInfos.data();
	pipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroupCreateInfos.size());
	pipelineCreateInfo.pGroups = shaderGroupCreateInfos.data();
	pipelineCreateInfo.maxPipelineRayRecursionDepth = 1;
	pipelineCreateInfo.layout = layout;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = 0;

	result = vkCreateRayTracingPipelinesKHR(ctx.logicalDevice, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	CheckVulkanResult("Failed to create the pipeline for ray tracing", result);

	vkDestroyShaderModule(ctx.logicalDevice, missShader, nullptr);
	vkDestroyShaderModule(ctx.logicalDevice, hitShader, nullptr);
	vkDestroyShaderModule(ctx.logicalDevice, genShader, nullptr);
}

void RayTracingPipeline::CreateShaderBindingTable()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{};
	rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rtProperties;

	vkGetPhysicalDeviceProperties2(ctx.physicalDevice.Device(), &properties2);

	VkDeviceSize progSize = rtProperties.shaderGroupBaseAlignment;
	VkDeviceSize shaderBindingTableSize = progSize * RT_GROUP_COUNT;

	shaderBindingTableBuffer.Init(shaderBindingTableSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	std::vector<char> shaderBuffer(shaderBindingTableSize);
	VkResult result = vkGetRayTracingShaderGroupHandlesKHR(ctx.logicalDevice, pipeline, 0, RT_GROUP_COUNT, shaderBindingTableSize, shaderBuffer.data());
	CheckVulkanResult("Failed to get the ray tracing shader group handles", result);

	char* shaderBindingTableMemPtr = shaderBindingTableBuffer.Map<char>(0, shaderBindingTableSize);

	for (uint32_t i = 0; i < RT_GROUP_COUNT; i++)
	{
		memcpy(shaderBindingTableMemPtr, shaderBuffer.data() + i * rtProperties.shaderGroupHandleSize, rtProperties.shaderGroupHandleSize);
		shaderBindingTableMemPtr = shaderBindingTableMemPtr + rtProperties.shaderGroupBaseAlignment;
	}
	shaderBindingTableBuffer.Unmap();

	VkDeviceAddress shaderBindingTableBufferAddress = shaderBindingTableBuffer.GetDeviceAddress();

	VkDeviceSize hitGroupOffset = 0;
	VkDeviceSize rayGenOffset = progSize;
	VkDeviceSize missOffset = progSize * 2;

	rchitShaderBinding.deviceAddress = shaderBindingTableBufferAddress + hitGroupOffset;
	rchitShaderBinding.size = progSize;
	rchitShaderBinding.stride = progSize;

	rgenShaderBinding.deviceAddress = shaderBindingTableBufferAddress + rayGenOffset;
	rgenShaderBinding.size = progSize;
	rgenShaderBinding.stride = progSize;

	rmissShaderBinding.deviceAddress = shaderBindingTableBufferAddress + missOffset;
	rmissShaderBinding.size = progSize;
	rmissShaderBinding.stride = progSize;
}

void RayTracingPipeline::Execute(const CommandBuffer& cmdBuffer, uint32_t width, uint32_t height, uint32_t depth) const
{
	cmdBuffer.TraceRays(rgenShaderBinding, rmissShaderBinding, rchitShaderBinding, rmissShaderBinding, width, height, depth);
}