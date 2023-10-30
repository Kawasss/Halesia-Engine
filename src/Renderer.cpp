#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <array>
#include <chrono>
#include "renderer/Vulkan.h"
#include "renderer/Swapchain.h"
#include "renderer/Surface.h"
#include "renderer/PipelineBuilder.h"
#include "renderer/Texture.h"
#include "vulkan/vk_enum_string_helper.h"
#include "system/SystemMetrics.h"
#include "system/Window.h"
#include "system/Input.h"
#include "Console.h"

#define IMGUI_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui-1.89.8/imgui-1.89.8/implot.cpp"
#include "imgui-1.89.8/imgui-1.89.8/misc/cpp/imgui_stdlib.cpp"
#include "imgui-1.89.8/imgui-1.89.8/implot_items.cpp"
#include "imgui-1.89.8/imgui-1.89.8/misc/single_file/imgui_single_file.h"
#include "imgui-1.89.8/imgui-1.89.8/backends/imgui_impl_vulkan.cpp"
#include "imgui-1.89.8/imgui-1.89.8/backends/imgui_impl_win32.cpp"

#include "renderer/Renderer.h"

#define nameof(s) #s
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define VariableToString(name) variableToString(#name)
std::string variableToString(const char* name)
{
	return name; //gimmicky? yes
}

ApeironBuffer<Vertex> Renderer::globalVertexBuffer;
ApeironBuffer<uint16_t> Renderer::globalIndicesBuffer;
bool Renderer::initGlobalBuffers = false;

struct UniformBufferObject
{
	glm::vec3 cameraPos;
	
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};

VkMemoryAllocateFlagsInfo allocateFlagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0 };

Renderer::Renderer(Win32Window* window)
{
	testWindow = window;
	window->additionalPollCallback = ImGui_ImplWin32_WndProcHandler;
	Vulkan::optionalMemoryAllocationFlags = &allocateFlagsInfo;
	InitVulkan();
}

void Renderer::Destroy()
{
	vkDeviceWaitIdle(logicalDevice);

	rayTracer->Destroy();

	vkDestroyDescriptorPool(logicalDevice, imGUIDescriptorPool, nullptr);
	ImGui_ImplVulkan_Shutdown();

	swapchain->Destroy();

	vkDestroySampler(logicalDevice, textureSampler, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroyBuffer(logicalDevice, modelBuffers[i], nullptr);
		vkFreeMemory(logicalDevice, modelBuffersMemory[i], nullptr);
	}

	Texture::DestroyPlaceholderTextures();

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

	vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(logicalDevice, imageAvaibleSemaphores[i], nullptr);
		vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);

		vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

	vkDestroyDevice(logicalDevice, nullptr);

	surface.Destroy();

	vkDestroyInstance(instance, nullptr);
	delete this;
}

VulkanCreationObject Renderer::GetVulkanCreationObject()
{
	return creationObject;
}

void Renderer::CreateImGUI()
{
	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolCreateInfo.maxSets = 1000;
	poolCreateInfo.poolSizeCount = std::size(poolSizes);
	poolCreateInfo.pPoolSizes = poolSizes;

	VkResult result = vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &imGUIDescriptorPool);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the descriptor pool for imGUI", result, nameof(vkCreateDescriptorPool), __FILENAME__, std::to_string(__LINE__));

	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGui_ImplWin32_Init(testWindow->window);

	ImGui_ImplVulkan_InitInfo imGUICreateInfo{};
	imGUICreateInfo.Instance = instance;
	imGUICreateInfo.PhysicalDevice = physicalDevice.Device();
	imGUICreateInfo.Device = logicalDevice;
	imGUICreateInfo.Queue = graphicsQueue;
	imGUICreateInfo.DescriptorPool = imGUIDescriptorPool;
	imGUICreateInfo.MinImageCount = 3;
	imGUICreateInfo.ImageCount = 3;
	imGUICreateInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&imGUICreateInfo, renderPass);

	VkCommandBuffer imGUICommandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
	ImGui_ImplVulkan_CreateFontsTexture(imGUICommandBuffer);
	Vulkan::EndSingleTimeCommands(logicalDevice, graphicsQueue, imGUICommandBuffer, commandPool);

	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Renderer::InitVulkan()
{
	Console::commandVariables["raySamples"] = &RayTracing::raySampleCount;
	Console::commandVariables["rayDepth"] = &RayTracing::rayDepth;
	Console::commandVariables["rasterize"] = &shouldRasterize;
	Console::commandVariables["showNormals"] = &RayTracing::showNormals;

	instance = Vulkan::GenerateInstance();
	surface = Surface::GenerateSurface(instance, testWindow);
	physicalDevice = Vulkan::GetBestPhysicalDevice(instance, surface);
	SetLogicalDevice();
	creationObject = { logicalDevice, physicalDevice, graphicsQueue, queueIndex };
	swapchain = new Swapchain(logicalDevice, physicalDevice, surface, testWindow);
	swapchain->CreateImageViews();
	CreateDescriptorSetLayout();
	CreateGraphicsPipeline();
	CreateCommandPool();
	swapchain->CreateDepthBuffers();
	swapchain->CreateFramebuffers(renderPass);
	Texture::GeneratePlaceholderTextures(GetVulkanCreationObject());
	CreateTextureSampler();
	CreateUniformBuffers();
	CreateModelBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffer();
	CreateSyncObjects();
	CreateImGUI();

	if (!initGlobalBuffers)
	{
		globalVertexBuffer.Reserve(creationObject, 10000, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		globalIndicesBuffer.Reserve(creationObject, 10000, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		initGlobalBuffers = true;
	}

	rayTracer = new RayTracing();
}

std::vector<VkDynamicState> dynamicStates =
{
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
};

void Renderer::CreateTextureSampler()
{
	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.anisotropyEnable = VK_TRUE;

	VkPhysicalDeviceProperties deviceProperties = physicalDevice.Properties();
	createInfo.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
	createInfo.unnormalizedCoordinates = VK_FALSE;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias = 0;
	createInfo.minLod = 0;
	createInfo.maxLod = VK_LOD_CLAMP_NONE;//static_cast<uint32_t>(textureImage->GetMipLevels());

	VkResult result = vkCreateSampler(logicalDevice, &createInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the texture sampler", result, nameof(vkCreateSampler), __FILENAME__, std::to_string(__LINE__));
}

void Renderer::CreateModelBuffers()
{
	VkDeviceSize size = sizeof(glm::mat4) * MAX_MESHES;
	
	modelBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	modelBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	modelBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		Vulkan::CreateBuffer(logicalDevice, physicalDevice, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, modelBuffers[i], modelBuffersMemory[i]);
		vkMapMemory(logicalDevice, modelBuffersMemory[i], 0, size, 0, &modelBuffersMapped[i]);
	}
}

void Renderer::SetModelMatrices(uint32_t currentImage, std::vector<Object*> models)
{
	std::vector<glm::mat4> modelMatrices;
	for (Object* model : models)
	{
		if (model->state != STATUS_VISIBLE || !model->HasFinishedLoading())
			continue;

		glm::mat4 modelMatrix = model->transform.GetModelMatrix();
		for (Mesh mesh : model->meshes)
			modelMatrices.push_back(modelMatrix);
	}
	memcpy(modelBuffersMapped[currentImage], modelMatrices.data(), modelMatrices.size() * sizeof(glm::mat4));
}

void Renderer::CreateUniformBuffers()
{
	VkDeviceSize size = sizeof(UniformBufferObject);

	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		Vulkan::CreateBuffer(logicalDevice, physicalDevice, size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		vkMapMemory(logicalDevice, uniformBuffersMemory[i], 0, size, 0, &uniformBuffersMapped[i]);
	}
}

void Renderer::CreateRenderPass()
{
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain->format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = physicalDevice.GetDepthFormat();
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
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	renderPassInfo.pAttachments = attachmentDescriptions.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkResult result = vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create a render pass", result, nameof(vkCreateRenderPass), __FILENAME__, std::to_string(__LINE__));
}

void Renderer::CreateDescriptorSets()
{
	std::vector<uint32_t> setCounts;
	setCounts.push_back(MAX_FRAMES_IN_FLIGHT);
	setCounts.push_back(MAX_FRAMES_IN_FLIGHT);
	setCounts.push_back(MAX_FRAMES_IN_FLIGHT * MAX_BINDLESS_TEXTURES);

	VkDescriptorSetVariableDescriptorCountAllocateInfoEXT countAllocateInfo{};
	countAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
	countAllocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	countAllocateInfo.pDescriptorCounts = setCounts.data();

	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocateInfo.pSetLayouts = layouts.data();
	allocateInfo.pNext = &countAllocateInfo;

	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

	VkResult result = vkAllocateDescriptorSets(logicalDevice, &allocateInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to allocate the descriptor sets", result, nameof(vkAllocateDescriptorSets), __FILENAME__, std::to_string(__LINE__));

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorBufferInfo modelBufferInfo{};
		modelBufferInfo.buffer = modelBuffers[i];
		modelBufferInfo.offset = 0;
		modelBufferInfo.range = sizeof(glm::mat4) * MAX_MESHES;

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = VK_NULL_HANDLE;
		imageInfo.sampler = textureSampler;

		std::array<VkWriteDescriptorSet, 3> writeDescriptorSets{};
		writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[0].dstSet = descriptorSets[i];
		writeDescriptorSets[0].dstBinding = 0;
		writeDescriptorSets[0].dstArrayElement = 0;
		writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSets[0].descriptorCount = 1;
		writeDescriptorSets[0].pBufferInfo = &bufferInfo;

		writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[1].dstSet = descriptorSets[i];
		writeDescriptorSets[1].dstBinding = 1;
		writeDescriptorSets[1].dstArrayElement = 0;
		writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSets[1].descriptorCount = 1;
		writeDescriptorSets[1].pBufferInfo = &modelBufferInfo;

		writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[2].dstSet = descriptorSets[i];
		writeDescriptorSets[2].dstBinding = 2;
		writeDescriptorSets[2].dstArrayElement = 0;
		writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSets[2].descriptorCount = 1;
		writeDescriptorSets[2].pImageInfo = &imageInfo;
		
		vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void Renderer::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[2].descriptorCount = MAX_FRAMES_IN_FLIGHT * MAX_BINDLESS_TEXTURES;

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes = poolSizes.data();
	createInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;

	VkResult result = vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the descriptor pool", result, nameof(vkCreateDescriptorPool), __FILENAME__, std::to_string(__LINE__));
}

void Renderer::CreateDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = 0;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding modelLayoutBinding{};
	modelLayoutBinding.binding = 1;
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	modelLayoutBinding.descriptorCount = 1;
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	modelLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 2;
	samplerLayoutBinding.descriptorCount = MAX_BINDLESS_TEXTURES;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 3> bindings = { layoutBinding, modelLayoutBinding, samplerLayoutBinding };

	std::vector<VkDescriptorBindingFlags> bindingFlags;
	bindingFlags.push_back(0);
	bindingFlags.push_back(0);
	bindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

	VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlagsCreateInfo{};
	bindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
	bindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	bindingFlagsCreateInfo.pBindingFlags = bindingFlags.data();

	VkDescriptorSetLayoutCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	createInfo.pBindings = bindings.data();
	createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	createInfo.pNext = &bindingFlagsCreateInfo;

	VkResult result = vkCreateDescriptorSetLayout(logicalDevice, &createInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create the descriptor set layout", result, nameof(vkCreateDescriptorSetLayout), __FILENAME__, std::to_string(__LINE__));
}

void Renderer::CreateGraphicsPipeline()
{
	std::vector<char> vertexShaderSource = ReadFile("shaders/vert.spv");
	std::vector<char> fragmentShaderSource = ReadFile("shaders/frag.spv");

	VkShaderModule vertexShaderModule = Vulkan::CreateShaderModule(logicalDevice, vertexShaderSource);
	VkShaderModule fragmentShaderModule = Vulkan::CreateShaderModule(logicalDevice, fragmentShaderSource);

	VkPipelineShaderStageCreateInfo vertexCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(vertexShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
	VkPipelineShaderStageCreateInfo fragmentCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(fragmentShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexCreateInfo, fragmentCreateInfo };

	//create pipeline layout
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();
	dynamicState.pNext = nullptr;
	dynamicState.flags = 0;

	VkVertexInputBindingDescription bindingDescription = Vertex::GetBindingDescription();
	std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions = Vertex::GetAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.vertexBindingDescriptionCount = 0;

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = false;

	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = (float)swapchain->extent.width;
	viewport.height = (float)swapchain->extent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1;

	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchain->extent;
	PipelineBuilder builder{};

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;

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
	multisampling.sampleShadingEnable = false;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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

	VkResult result = vkCreatePipelineLayout(logicalDevice, &layoutInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create a pipeline layout", result, nameof(vkCreatePipelineLayout), __FILENAME__, std::to_string(__LINE__));
		
	CreateRenderPass();
	
	//graphicsPipeline = builder.BuildGraphicsPipeline(logicalDevice, descriptorSetLayout, renderPass, pipelineLayout, vertexShaderSource, fragmentShaderSource, dynamicStates, viewport, scissor);

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

	result = vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to create a graphics pipeline", result, nameof(vkCreateGraphicsPipelines), __FILENAME__, std::to_string(__LINE__));

	vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
}

std::vector<char> Renderer::ReadFile(const std::string& filePath)
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

void Renderer::SetLogicalDevice()
{
	logicalDevice = physicalDevice.GetLogicalDevice(surface);
	QueueFamilyIndices indices = physicalDevice.QueueFamilies(surface);
	queueIndex = indices.graphicsFamily.value();

	vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
}

void Renderer::CreateCommandPool()
{
	commandPool = Vulkan::FetchNewCommandPool(GetVulkanCreationObject());
}

void Renderer::CreateCommandBuffer()
{
	commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocationInfo{};
	allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocationInfo.commandPool = commandPool;
	allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocationInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	VkResult result = vkAllocateCommandBuffers(logicalDevice, &allocationInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
		throw VulkanAPIError("Failed to allocate the command buffer", result, nameof(vkAllocateCommandBuffers), __FILENAME__, std::to_string(__LINE__));
}

void Renderer::SetViewport(VkCommandBuffer commandBuffer)
{
	VkViewport viewport{};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = static_cast<uint32_t>(swapchain->extent.width);
	viewport.height = static_cast<uint32_t>(swapchain->extent.height);
	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
}

void Renderer::SetScissors(VkCommandBuffer commandBuffer)
{
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = swapchain->extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

bool submittedObjects = false; // this is ABSOLUTELY not the way to do this, this is only temporary and should be handled in object.cpp, not here

bool initRT = false;
void Renderer::RecordCommandBuffer(VkCommandBuffer lCommandBuffer, uint32_t imageIndex, std::vector<Object*> objects, Camera* camera)
{
	if (!initRT)
	{
		rayTracer->Init(logicalDevice, physicalDevice, surface, objects[0], camera, testWindow, swapchain);
		initRT = true;
	} // not a good place to do this

	if (!submittedObjects && objects.size() == 2)
	{
		rayTracer->SubmitObjects(GetVulkanCreationObject(), objects);
		submittedObjects = true;
	}

	/*if (submittedObjects.size() == 0 && objects.size() > 0)
	{
		submittedObjects.push_back(objects[0]);
		rayTracer->SumbitObject(GetVulkanCreationObject(), objects[0]);
	}*/

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkResult result = vkBeginCommandBuffer(lCommandBuffer, &beginInfo);
	CheckVulkanResult("Failed to begin the given command buffer", result, nameof(vkBeginCommandBuffer));

	if (!shouldRasterize)
	{
		rayTracer->DrawFrame(objects, testWindow, camera, lCommandBuffer, imageIndex);
		swapchain->CopyImageToSwapchain(rayTracer->RTImage, lCommandBuffer, imageIndex);
	}

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

	vkCmdBeginRenderPass(lCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	if (shouldRasterize)
	{
		vkCmdBindPipeline(lCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		SetViewport(lCommandBuffer);
		SetScissors(lCommandBuffer);

		vkCmdBindDescriptorSets(lCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

		for (Object* object : objects)
			if (object->state == STATUS_VISIBLE && object->HasFinishedLoading())
				for (Mesh mesh : object->meshes)
				{
					VkBuffer vertexBuffers[] = { globalVertexBuffer.GetBufferHandle()/*mesh.vertexBuffer.GetVkBuffer()*/};
					VkDeviceSize offsets[] = { globalVertexBuffer.GetMemoryOffset(mesh.vertexMemory)/*0*/ };
					vkCmdBindVertexBuffers(lCommandBuffer, 0, 1, vertexBuffers, offsets);
					vkCmdBindIndexBuffer(lCommandBuffer, globalIndicesBuffer.GetBufferHandle(), globalIndicesBuffer.GetMemoryOffset(mesh.indexMemory),/*mesh.indexBuffer.GetVkBuffer(), 0,*/ VK_INDEX_TYPE_UINT16);

					vkCmdDrawIndexed(lCommandBuffer, static_cast<uint32_t>(mesh.indices.size()), 1, 0, 0, 0);
				}
	}

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), lCommandBuffer);

	vkCmdEndRenderPass(lCommandBuffer);

	result = vkEndCommandBuffer(lCommandBuffer);
	CheckVulkanResult("Failed to record / end the command buffer", result, nameof(vkEndCommandBuffer));
}

void Renderer::CreateSyncObjects()
{
	imageAvaibleSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvaibleSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &inFlightFences[i]))
			throw VulkanAPIError("Failed to create the required semaphores and fence", VK_SUCCESS, nameof(CreateSyncObjects), __FILENAME__, std::to_string(__LINE__)); // too difficult / annoying to put all of these calls into result = ...
	}
}

std::optional<std::string> Renderer::RenderDevConsole()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	std::optional<std::string> inputText;

	if (Console::isOpen)
	{
		ImGui::Begin("Dev Console", nullptr, ImGuiWindowFlags_NoCollapse);
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.9f);
		colors[ImGuiCol_Border] = ImVec4(0.05f, 0.05f, 0.05f, 1);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.05f, 0.05f, 0.05f, 1);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.49f, 0.68f, 1);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.56f, 0.49f, 0.68f, 1);

		style.WindowRounding = 5;
		style.WindowBorderSize = 2;

		for (std::string message : Console::messages)
		{
			glm::vec3 color = Console::GetColorFromMessage(message);
			ImGui::TextColored(ImVec4(color.x, color.y, color.z, 1), message.c_str());
		}

		std::string result = "";
		ImGui::InputTextWithHint("##input", "Console commands...", &result);

		if (Input::IsKeyPressed(VirtualKey::Return)) // if enter is pressed place the input value into the optional variable
			inputText = result;
		
		ImGui::End();
	}
	return inputText;
}

void Renderer::RenderFPS(int FPS)
{
	ImGui::Begin("##FPS Counter", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
	
	ImGui::SetWindowPos(ImVec2(0, 0/*ImGui::GetWindowSize().y * 0.2f*/));
	ImGui::SetWindowSize(ImVec2(ImGui::GetWindowSize().x * 2, ImGui::GetWindowSize().y));
	std::string text = std::to_string(FPS) + " FPS";
	ImGui::Text(text.c_str());

	ImGui::End();
}

void SetImGuiColors()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 5;
	style.WindowBorderSize = 2;

	ImVec4* colors = style.Colors;

	colors[ImGuiCol_WindowBg] = ImVec4(0.01f, 0.01f, 0.01f, 0.9f);
	colors[ImGuiCol_Border] = ImVec4(0.05f, 0.05f, 0.05f, 1);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1);
}

void Renderer::RenderPieGraph(std::vector<float>& data, const char* label)
{
	ImGui::Begin(label, nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();
	
	ImPlot::BeginPlot("##Time Per Async Task", ImVec2(-1, 0), ImPlotFlags_Equal | ImPlotFlags_NoMouseText | ImPlotFlags_NoFrame);
	ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
	ImPlot::SetupAxesLimits(0, 1, 0, 1);
	const char* labels[] = { "Main Thread", "Script Thread", "Renderer Thread" };
	ImPlot::PlotPieChart(labels, data.data(), 3, 0.5, 0.5, 0.5, "%.1f", 180);
	ImPlot::EndPlot();
	ImGui::End();
}

void Renderer::RenderGraph(const std::vector<uint64_t>& buffer, const char* label)
{
	ImGui::Begin("RAM Usage", nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	ImPlot::BeginPlot("##Ram Usage Over Time", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly);
	ImPlot::SetupAxisLimits(ImAxis_X1, 0, 500);
	ImPlot::SetupAxisLimits(ImAxis_Y1, 0, buffer[buffer.size() - 1] * 1.3);
	ImPlot::SetupAxes("##x", "##y", ImPlotAxisFlags_NoTickLabels);
	ImPlot::PlotLine(label, buffer.data(), buffer.size());
	ImPlot::EndPlot();
	ImGui::End();
}

void Renderer::RenderGraph(const std::vector<float>& buffer, const char* label)
{
	ImGui::Begin(label, nullptr, ImGuiWindowFlags_NoScrollbar);
	SetImGuiColors();

	ImPlot::BeginPlot("##Ram Usage Over Time", ImVec2(-1, 0), ImPlotFlags_NoInputs | ImPlotFlags_NoFrame | ImPlotFlags_CanvasOnly);
	ImPlot::SetupAxisLimits(ImAxis_X1, 0, 100);
	ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 100);
	ImPlot::SetupAxes("##x", "##y", ImPlotAxisFlags_NoTickLabels);
	ImPlot::PlotLine(label, buffer.data(), buffer.size());
	ImPlot::EndPlot();
	ImGui::End();
}

void Renderer::DrawFrame(const std::vector<Object*>& objects, Camera* camera, float delta)
{
	ImGui::Render();

	vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], true, UINT64_MAX);
	Vulkan::graphicsQueueMutex->lock();

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(logicalDevice, swapchain->vkSwapchain, UINT64_MAX, imageAvaibleSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		swapchain->Recreate(renderPass);
		rayTracer->RecreateImage(swapchain);
		testWindow->resized = false;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		throw VulkanAPIError("Failed to acquire the next swap chain image", result, nameof(vkAcquireNextImageKHR), __FILENAME__, std::to_string(__LINE__));

	vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

	UpdateUniformBuffers(currentFrame, camera);
	SetModelMatrices(currentFrame, objects);

	//UpdateBindlessTextures(currentFrame, objects);

	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	RecordCommandBuffer(commandBuffers[currentFrame], imageIndex, objects, camera);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvaibleSemaphores[currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
	CheckVulkanResult("Failed to submit the queue", result, nameof(vkQueueSubmit));

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchains[] = { swapchain->vkSwapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(presentQueue, &presentInfo);
	Vulkan::graphicsQueueMutex->unlock();
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || testWindow->resized)
	{
		swapchain->Recreate(renderPass);
		rayTracer->RecreateImage(swapchain);
		testWindow->resized = false;
		Console::WriteLine("Resized to " + std::to_string(testWindow->GetWidth()) + 'x' + std::to_string(testWindow->GetHeight()) + " px");
	}
	else CheckVulkanResult("Failed to present the swap chain image", result, nameof(vkQueuePresentKHR));
	
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

std::vector<Object*> processedObjects;
void Renderer::UpdateBindlessTextures(uint32_t currentFrame, const std::vector<Object*>& objects)
{
	if (!Image::TexturesHaveChanged()) // disabled since there is a racing condition for the meshes. If the meshes are done loading after the textures, the textures get written off as being updated, whilst the meshes material isnt
		return;
		
	// !! this for loop currently updates the textures for all descriptor sets at once which is not that good. itd be better to update the currently used descriptor set
	std::vector<VkDescriptorImageInfo> imageInfos;

	for (Object* object : objects) // its wasteful to update the textures of all the meshes if those arent changed, its wasting resources. its better to have a look up table or smth like that to look up if a material has already been updated, dstArrayElement also needs to be made dynamic for that
		for (Mesh mesh : object->meshes)
			for (int i = 0; i < 5; i++) // 5 textures per material (using 2 now because the rest aren't implemented yet)
			{
				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = mesh.material[i]->imageView;
				imageInfo.sampler = textureSampler;

				imageInfos.push_back(imageInfo);
	}
	if (imageInfos.size() == 0)
		return;

	// this for loop updates every descriptor set with the textures that are only really relevant for the current frame, this can be wasteful if there are textures that stop being used after this frame. this cant be changed easily because of Image::TexturesHaveChanged
	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkWriteDescriptorSet writeSet{};
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeSet.descriptorCount = static_cast<uint32_t>(imageInfos.size());
		writeSet.dstBinding = 2;
		writeSet.dstSet = descriptorSets[i];
		writeSet.pImageInfo = imageInfos.data();
		writeSet.dstArrayElement = 0; // should be made dynamic if this function no longer updates from the beginning of the array
		
		vkUpdateDescriptorSets(logicalDevice, 1, &writeSet, 0, nullptr);
	}
}

void Renderer::UpdateUniformBuffers(uint32_t currentImage, Camera* camera)
{
	UniformBufferObject ubo{};
	ubo.cameraPos = camera->position;
	ubo.view = camera->GetViewMatrix();
	ubo.projection = camera->GetProjectionMatrix();
	memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}