#include "renderer/Vulkan.h"

#include "renderer/GraphicsPipeline.h"
#include "renderer/PipelineCreator.h"
#include "renderer/ShaderReflector.h"
#include "renderer/VulkanAPIError.h"
#include "renderer/ShaderCompiler.h"
#include "renderer/Renderer.h"

#include "io/IO.h"

GraphicsPipeline::GraphicsPipeline(const CreateInfo& createInfo)
{
	std::expected<CompiledShader, bool> vertex   = ShaderCompiler::Compile(createInfo.vertexShader);
	std::expected<CompiledShader, bool> fragment = ShaderCompiler::Compile(createInfo.fragmentShader);

	if (!vertex.has_value() || !fragment.has_value())
		return; // throw..?

	std::array<std::span<char>, 2> shaderCodes =
	{
		vertex->code,
		fragment->code,
	};

	ShaderGroupReflector reflector(shaderCodes);
	for (uint32_t set : vertex->externalSets)
		reflector.ExcludeSet(set);
	for (uint32_t set : fragment->externalSets)
		reflector.ExcludeSet(set);

	InitializeBase(reflector);

	CreatePipelineLayout(reflector);
	CreateGraphicsPipeline(shaderCodes, createInfo, reflector.GetOutputVariableCount(1));

	bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	Vulkan::SetDebugName(pipeline, "graphics pipeline");
	Vulkan::SetDebugName(layout, "graphics pipeline layout");
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
	CheckVulkanResult("Failed to create a pipeline layout for a graphics pipeline", result);
}

void GraphicsPipeline::CreateGraphicsPipeline(const std::span<std::span<char>>& shaders, const CreateInfo& createInfo, uint32_t attachmentCount)
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
	builder.topology = createInfo.topology;

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