#include <map>

#include "renderer/Vulkan.h"

#include "renderer/GraphicsPipeline.h"
#include "renderer/PipelineCreator.h"
#include "renderer/ShaderReflector.h"
#include "renderer/GarbageManager.h"
#include "renderer/Buffer.h"

#include "io/IO.h"

GraphicsPipeline::GraphicsPipeline(const CreateInfo& createInfo)
{
	std::vector<std::vector<char>> shaderCodes =
	{
		IO::ReadFile(createInfo.vertexShader),   // vert is [0]
		IO::ReadFile(createInfo.fragmentShader), // frag is [1]
	};
	ShaderGroupReflector reflector(shaderCodes);

	InitializeBase(reflector);

	CreatePipelineLayout(reflector);
	CreateGraphicsPipeline(shaderCodes, createInfo, reflector.GetOutputVariableCount(1));

	bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
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

void GraphicsPipeline::CreateGraphicsPipeline(const std::vector<std::vector<char>>& shaders, const CreateInfo& createInfo, uint32_t attachmentCount)
{
	VkShaderModule vertModule = Vulkan::CreateShaderModule(shaders[0]);
	VkShaderModule fragModule = Vulkan::CreateShaderModule(shaders[1]);

	std::vector<VkPipelineShaderStageCreateInfo> shaderInfos = 
	{
		Vulkan::GetGenericShaderStageCreateInfo(vertModule, VK_SHADER_STAGE_VERTEX_BIT),
		Vulkan::GetGenericShaderStageCreateInfo(fragModule, VK_SHADER_STAGE_FRAGMENT_BIT),
	};

	PipelineBuilder builder(shaderInfos);

	builder.layout = layout;
	builder.attachmentCount = attachmentCount;
	builder.renderPass = createInfo.renderPass;
	builder.depthCompareOp = createInfo.depthCompareOp;

	builder.DisableVertices(createInfo.noVertices);
	builder.DisableDepth(createInfo.noDepth);
	builder.DisableCulling(createInfo.noCulling);
	builder.DisableBlending(createInfo.noBlending);
	builder.ShouldCullFront(createInfo.cullFront);
	builder.FrontIsCW(createInfo.frontCW);
	builder.PolygonAsLine(createInfo.polygonLine);
	builder.WriteToDepth(createInfo.writeDepth);

	pipeline = builder.Build();

	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkDestroyShaderModule(ctx.logicalDevice, vertModule, nullptr);
	vkDestroyShaderModule(ctx.logicalDevice, fragModule, nullptr);
}