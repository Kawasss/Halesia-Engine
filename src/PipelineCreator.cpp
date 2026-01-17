module;

#include "renderer/Vulkan.h"
#include "renderer/PhysicalDevice.h"
#include "renderer/Vertex.h"
#include "renderer/VulkanAPIError.h"

module Renderer.PipelineCreator;

import std;

VkPipeline PipelineBuilder::Build()
{
	if (renderPass == VK_NULL_HANDLE && pNext == nullptr)
		throw VulkanAPIError("Cannot build a pipeline: the render pass is invalid and no pNext was given.");

	VkPipelineDynamicStateCreateInfo dynamicState = Vulkan::GetDynamicStateCreateInfo();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	std::array<VkVertexInputAttributeDescription, 7> attributeDescriptions = Vertex::GetAttributeDescriptions();
	VkVertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
	if (!options.noVertex)
	{
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = topology;
	inputAssembly.primitiveRestartEnable = false;

	VkPipelineViewportStateCreateInfo viewportState = Vulkan::GetDynamicViewportStateCreateInfo();

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthCompareOp = depthCompareOp;
	depthStencil.depthTestEnable  = !options.noDepth;
	depthStencil.depthWriteEnable = options.writeDepth;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.back.compareMask = VK_COMPARE_OP_ALWAYS;

	depthStencil.maxDepthBounds = 1.0f;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = options.polygonLine ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = options.noCulling ? VK_CULL_MODE_NONE : (options.cullFront ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
	rasterizer.frontFace = options.frontCW ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.lineWidth = 1;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment{};

	blendAttachment.blendEnable = !options.noBlend;
	if (blendAttachment.blendEnable)
	{
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(attachmentCount, blendAttachment);

	VkPipelineColorBlendStateCreateInfo blendCreateInfo{};
	blendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	blendCreateInfo.logicOpEnable = VK_FALSE;
	blendCreateInfo.pAttachments = blendAttachments.data();
	blendCreateInfo.attachmentCount = attachmentCount;

	VkGraphicsPipelineCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.pNext = pNext;
	createInfo.stageCount = 2;
	createInfo.pStages = stages.data();
	createInfo.pVertexInputState = &vertexInputInfo;
	createInfo.pInputAssemblyState = &inputAssembly;
	createInfo.pViewportState = &viewportState;
	createInfo.pRasterizationState = &rasterizer;
	createInfo.pMultisampleState = &multisampling;
	createInfo.pColorBlendState = &blendCreateInfo;
	createInfo.pDynamicState = &dynamicState;
	createInfo.pDepthStencilState = &depthStencil;
	createInfo.layout = layout;
	createInfo.renderPass = renderPass;
	createInfo.subpass = 0;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkResult result = vkCreateGraphicsPipelines(Vulkan::GetContext().logicalDevice, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline);
	CheckVulkanResult("Failed to create a graphics pipeline", result);

	return pipeline;
}

VkRenderPass RenderPassBuilder::Build()
{
	if (initialLayout == INVALID_LAYOUT || finalLayout == INVALID_LAYOUT)
		throw VulkanAPIError("Cannot build a render pass: the initial or final layout is invalid.");

	const Vulkan::Context& ctx = Vulkan::GetContext();

	uint32_t attachmentCount = static_cast<uint32_t>(formats.size());

	std::vector<VkAttachmentDescription> attachments(attachmentCount);
	std::vector<VkAttachmentReference> colorReferences(attachmentCount);

	for (uint32_t i = 0; i < attachmentCount; i++)
	{
		attachments[i].format = formats[i];
		attachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[i].loadOp = options.clearOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].initialLayout = initialLayout;
		attachments[i].finalLayout = finalLayout;

		colorReferences[i].attachment = i;
		colorReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}

	if (!options.noDepth)
	{
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = ctx.physicalDevice.GetDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = options.dontClearDepth ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		attachments.push_back(depthAttachment);
	}

	VkAttachmentReference depthReference{};
	depthReference.attachment = attachmentCount;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = attachmentCount;
	subpass.pColorAttachments = colorReferences.data();
	subpass.pDepthStencilAttachment = options.noDepth ? nullptr : &depthReference;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcAccessMask = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkResult result = vkCreateRenderPass(ctx.logicalDevice, &renderPassInfo, nullptr, &renderPass);
	CheckVulkanResult("Failed to create a render pass", result);

	return renderPass;
}