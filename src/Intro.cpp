#include "renderer/Intro.h"
#include <fstream>
#include <array>
#include "renderer/Renderer.h"
#include "renderer/Swapchain.h"
#include "renderer/Texture.h"
#include "renderer/Buffers.h"

struct Timer
{
	float completionPercentage;
};

std::vector<char> Intro::ReadFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		throw std::runtime_error("Failed to open the shader at " + filePath);

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

void Intro::Create(VulkanCreationObject& creationObject, Swapchain* swapchain, std::string imagePath)
{
	this->logicalDevice = creationObject.logicalDevice;
	this->swapchain = swapchain;
	this->texture = new Texture(creationObject, imagePath);
	this->texture->AwaitGeneration();

	// descriptor pool

	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = 1;
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCreateInfo.pPoolSizes = poolSizes.data();

	VkResult result = vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create the descriptor pool for the intro", result, vkCreateDescriptorPool);

	// descriptor set layout

	std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings{};
	layoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // texture image
	layoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layoutBindings[0].binding = 0;
	layoutBindings[0].descriptorCount = 1;
	layoutBindings[0].pImmutableSamplers = nullptr;

	layoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // uniform buffer
	layoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layoutBindings[1].binding = 1;
	layoutBindings[1].descriptorCount = 1;
	layoutBindings[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo{};
	setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
	setLayoutCreateInfo.pBindings = layoutBindings.data();

	result = vkCreateDescriptorSetLayout(logicalDevice, &setLayoutCreateInfo, nullptr, &setLayout);
	CheckVulkanResult("Failed to create the descriptor set layout for the intro", result, vkCreateDescriptorSetLayout);

	// allocate descriptor set

	VkDescriptorSetAllocateInfo setAllocateInfo{};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = descriptorPool;
	setAllocateInfo.descriptorSetCount = 1;
	setAllocateInfo.pSetLayouts = &setLayout;

	result = vkAllocateDescriptorSets(logicalDevice, &setAllocateInfo, &descriptorSet);
	CheckVulkanResult("Faield to allocate the descriptor set for the intro", result, vkAllocateDescriptorSets);

	// shaders

	std::vector<char> vertCode = ReadFile("shaders/intro.vert.spv");
	std::vector<char> fragCode = ReadFile("shaders/intro.frag.spv");

	VkShaderModule vertexShader = Vulkan::CreateShaderModule(logicalDevice, vertCode);
	VkShaderModule fragShader = Vulkan::CreateShaderModule(logicalDevice, fragCode);
	
	// pipeline

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos
	{
		Vulkan::GetGenericShaderStageCreateInfo(vertexShader, VK_SHADER_STAGE_VERTEX_BIT), // vertex
		Vulkan::GetGenericShaderStageCreateInfo(fragShader, VK_SHADER_STAGE_FRAGMENT_BIT)  // fragment
	};

	// create pipeline layout

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(Renderer::dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = Renderer::dynamicStates.data();


	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.vertexBindingDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{};
	VkRect2D scissors{};
	Vulkan::PopulateDefaultViewport(viewport, swapchain);
	Vulkan::PopulateDefaultScissors(scissors, swapchain);

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissors;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.lineWidth = 1;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blendCreateInfo{};
	blendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendCreateInfo.logicOpEnable = VK_FALSE;
	blendCreateInfo.attachmentCount = 1;
	blendCreateInfo.pAttachments = &blendAttachment;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &setLayout;

	result = vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	CheckVulkanResult("Failed to create the pipeline layout for the intro", result, vkCreatePipelineLayout);

	// render pass

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain->format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = creationObject.physicalDevice.GetDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference{};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference{};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pDepthStencilAttachment = &depthReference;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcAccessMask = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachmentDescriptions = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	result = vkCreateRenderPass(logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	CheckVulkanResult("Failed to create the render pass for the intro", result, vkCreateRenderPass);

	// pipeline

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizer;
	pipelineCreateInfo.pMultisampleState = &multisampling;
	pipelineCreateInfo.pColorBlendState = &blendCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencil;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;

	result = vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	CheckVulkanResult("Failed to create the graphics pipeline for the intro", result, vkCreateGraphicsPipelines);

	vkDestroyShaderModule(logicalDevice, vertexShader, nullptr);
	vkDestroyShaderModule(logicalDevice, fragShader, nullptr);

	// uniform buffer

	Vulkan::CreateBuffer(logicalDevice, creationObject.physicalDevice, sizeof(Timer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, uniformBuffer, uniformBufferMemory);
	vkMapMemory(logicalDevice, uniformBufferMemory, 0, sizeof(Timer), 0, &uniformBufferPointer);

	// update descriptor set
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture->imageView;
	imageInfo.sampler = Renderer::defaultSampler;

	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = uniformBuffer;
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	std::array<VkWriteDescriptorSet, 2> writeSets{};
	writeSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSets[0].descriptorCount = 1;
	writeSets[0].dstArrayElement = 0;
	writeSets[0].dstBinding = 0;
	writeSets[0].dstSet = descriptorSet;
	writeSets[0].pImageInfo = &imageInfo;

	writeSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeSets[1].descriptorCount = 1;
	writeSets[1].dstArrayElement = 0;
	writeSets[1].dstBinding = 1;
	writeSets[1].dstSet = descriptorSet;
	writeSets[1].pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(logicalDevice, 2, writeSets.data(), 0, nullptr);
}

void Intro::WriteDataToBuffer(float timeElapsed)
{
	static Timer timer{ 0 };
	timer.completionPercentage = timeElapsed / timeForMaxTransparency;
	memcpy(uniformBufferPointer, &timer, sizeof(Timer)); // write the update percentage to the buffer
}

void Intro::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = swapchain->framebuffers[imageIndex];
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = swapchain->extent;

	std::array<VkClearValue, 2> clearColors{};
	clearColors[0].color = { 0, 0, 0, 1 };
	clearColors[1].depthStencil = { 1, 0 };

	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
	renderPassBeginInfo.pClearValues = clearColors.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)swapchain->extent.width;
	viewport.height = (float)swapchain->extent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchain->extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdDraw(commandBuffer, 6, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);
}

void Intro::Destroy()
{
	vkDeviceWaitIdle(logicalDevice);

	vkUnmapMemory(logicalDevice, uniformBufferMemory);
	vkDestroyBuffer(logicalDevice, uniformBuffer, nullptr);
	vkFreeMemory(logicalDevice, uniformBufferMemory, nullptr);

	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

	vkDestroyDescriptorSetLayout(logicalDevice, setLayout, nullptr);
	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);

	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
}