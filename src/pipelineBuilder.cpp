#include "renderer/PipelineBuilder.h"
#include "Vertex.h"
#include "spirv-reflect/common/output_stream.cpp"
#include "spirv-reflect/spirv_reflect.c"
#include <unordered_map>
#include <algorithm>
#include "glm.h"

std::unordered_map<SpvReflectFormat, uint32_t> sizeOfFormat{ { SPV_REFLECT_FORMAT_R32G32B32_SFLOAT, sizeof(glm::vec3) }, { SPV_REFLECT_FORMAT_R32G32_SFLOAT, sizeof(glm::vec2) }, { SPV_REFLECT_FORMAT_R32_SINT, sizeof(int) } };

SpvReflectShaderModule shaderModule;

void PipelineBuilder::CreateReflectShaderModule(const std::vector<char>& shaderSource)
{
	if (spvReflectCreateShaderModule(shaderSource.size(), static_cast<const void*>(shaderSource.data()), &shaderModule) != SPV_REFLECT_RESULT_SUCCESS)
		throw std::runtime_error("Failed to reflect on the given shader source code");
}

VkShaderModule PipelineBuilder::CreateShaderModule(VkDevice logicalDevice, const std::vector<char>& shaderSource)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = shaderSource.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderSource.data());

	VkShaderModule ret;
	if (vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &ret) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a shader module");
	return ret;
}

std::vector<const SpvReflectInterfaceVariable*> GetInputVariables(SpvReflectShaderModule module)
{
	std::vector<const SpvReflectInterfaceVariable*> ret;
	std::vector<SpvReflectInterfaceVariable*> unsorted;
	
	uint32_t inputVariableCount = 0;
	if (spvReflectEnumerateInputVariables(&module, &inputVariableCount, nullptr) != SPV_REFLECT_RESULT_SUCCESS)
		throw std::runtime_error("Failed to gather the amount of input variables from the given shader");
	
	unsorted.resize(inputVariableCount);
	if (spvReflectEnumerateInputVariables(&module, &inputVariableCount, unsorted.data()) != SPV_REFLECT_RESULT_SUCCESS)
		throw std::runtime_error("Failed to retrieve the input variables from the given shader");

	std::vector<uint32_t> locations;
	for (SpvReflectInterfaceVariable* variable : unsorted)
		locations.push_back(variable->location);

	std::sort(locations.begin(), locations.end());

	SpvReflectResult result;
	for (uint32_t location : locations)
		ret.push_back(spvReflectGetInputVariableByLocation(&module, location, &result));

	return ret;
}

VkVertexInputBindingDescription GetVertexInputBindingDescription(const std::vector<const SpvReflectInterfaceVariable*>& inputVariables)
{
	VkVertexInputBindingDescription ret{};

	uint32_t stride = 0;
	for (const SpvReflectInterfaceVariable* interfaceVariable : inputVariables)
	{
		//std::cout << ToStringFormat(interfaceVariable->format) << std::endl;
		//std::cout << interfaceVariable->name << std::endl;
		stride += sizeOfFormat[interfaceVariable->format];
	}
		
	ret.binding = 0;
	ret.stride = stride;
	ret.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	return ret;
}

std::vector<VkVertexInputAttributeDescription> GetVertexInputAttributeDescription(const std::vector<const SpvReflectInterfaceVariable*>& inputVariables) // would be better for performance to merge this with GetVertexInputBindingDescription, since both loop through the input variables
{
	std::vector<VkVertexInputAttributeDescription> ret;

	uint32_t offset = 0;
	for (const SpvReflectInterfaceVariable* inputVariable : inputVariables)
	{
		VkVertexInputAttributeDescription attributeDescription{};
		attributeDescription.binding = 0;
		attributeDescription.location = inputVariable->location;
		attributeDescription.format = (VkFormat)inputVariable->format;
		attributeDescription.offset = offset;
		offset += sizeOfFormat[inputVariable->format];
		ret.push_back(attributeDescription);
	}
	return ret;
}

VkPipeline PipelineBuilder::BuildGraphicsPipeline(VkDevice logicalDevice, VkDescriptorSetLayout descriptorSetLayout, VkRenderPass renderPass, VkPipelineLayout& pipelineLayout, std::vector<char>& vertexShaderSource, std::vector<char> fragmentShaderSource, std::vector<VkDynamicState> dynamicStates, VkViewport viewport, VkRect2D scissor)
{
	VkPipeline pipeline;

	CreateReflectShaderModule(vertexShaderSource);
	
	// todo: shader stages
	VkShaderModule vertexShaderModule = CreateShaderModule(logicalDevice, vertexShaderSource);
	VkShaderModule fragmentShaderModule = CreateShaderModule(logicalDevice, fragmentShaderSource);

	VkPipelineShaderStageCreateInfo vertexCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(vertexShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
	VkPipelineShaderStageCreateInfo fragmentCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(fragmentShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

	std::vector<const SpvReflectInterfaceVariable*> inputVariables = GetInputVariables(shaderModule);

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexCreateInfo, fragmentCreateInfo };

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();
	dynamicState.flags = 0;

	// todo: binding and attribute description
	VkVertexInputBindingDescription bindingDescription = GetVertexInputBindingDescription(inputVariables);
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions = GetVertexInputAttributeDescription(inputVariables);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;
	
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthBoundsTestEnable = VK_TRUE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = false;
	rasterizer.rasterizerDiscardEnable = false;
	rasterizer.lineWidth = 1;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = false;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachment.blendEnable = false;

	VkPipelineColorBlendStateCreateInfo blendCreateInfo{};
	blendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendCreateInfo.logicOpEnable = VK_FALSE;
	blendCreateInfo.attachmentCount = 1;
	blendCreateInfo.pAttachments = &blendAttachment;

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;

	if (vkCreatePipelineLayout(logicalDevice, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a pipeline layout");
	
	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizer;
	pipelineCreateInfo.pMultisampleState = &multisampling;
	pipelineCreateInfo.pColorBlendState = &blendCreateInfo;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.pDepthStencilState = &depthStencil;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS)
		throw std::runtime_error("Failed to create a graphics pipeline");

	vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);

	spvReflectDestroyShaderModule(&shaderModule);

	return pipeline;
}