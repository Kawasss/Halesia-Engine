#include <fstream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <array>
#include <chrono>
#include <filesystem>

#include "renderer/Vulkan.h"
#include "renderer/Swapchain.h"
#include "renderer/Surface.h"
#include "renderer/ShaderReflector.h"
#include "renderer/Texture.h"
#include "renderer/Intro.h"
#include "renderer/Mesh.h"
#include "renderer/RayTracing.h"

#include "system/SystemMetrics.h"
#include "system/Window.h"
#include "system/Input.h"

#include "core/Console.h"
#include "core/Object.h"
#include "core/Camera.h"

#define IMGUI_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#include "implot.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_win32.h"

#include "renderer/Renderer.h"

#define CheckCudaResult(result) if (result != cudaSuccess) throw std::runtime_error(std::to_string(result) + " at line " + std::to_string(__LINE__) + " in " + (std::string)__FILENAME__);
#define nameof(s) #s
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define VariableToString(name) variableToString(#name)
std::string variableToString(const char* name)
{
	return name; //gimmicky? yes
}

StorageBuffer<Vertex> Renderer::globalVertexBuffer;
StorageBuffer<uint16_t> Renderer::globalIndicesBuffer;
bool Renderer::initGlobalBuffers = false;
VkSampler Renderer::defaultSampler = VK_NULL_HANDLE;
Handle Renderer::selectedHandle = 0;
bool Renderer::shouldRenderCollisionBoxes = false;
bool Renderer::denoiseOutput = true;
float Renderer::internalScale = 1;

std::vector<VkDynamicState> Renderer::dynamicStates =
{
	VK_DYNAMIC_STATE_VIEWPORT,
	VK_DYNAMIC_STATE_SCISSOR
};

struct UniformBufferObject
{
	glm::vec3 cameraPos;
	
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};

struct ModelData
{
	glm::mat4 transformation;
	glm::vec4 IDColor;
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

	vkDestroySampler(logicalDevice, defaultSampler, nullptr);

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

	vkDestroyRenderPass(logicalDevice, deferredRenderPass, nullptr);
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
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
	poolCreateInfo.pPoolSizes = poolSizes;

	VkResult result = vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &imGUIDescriptorPool);
	CheckVulkanResult("Failed to create the descriptor pool for imGUI", result, vkCreateDescriptorPool);

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

	VkCommandBuffer imGUICommandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);
	ImGui_ImplVulkan_CreateFontsTexture(imGUICommandBuffer);
	Vulkan::EndSingleTimeCommands(graphicsQueue, imGUICommandBuffer, commandPool);

	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void Renderer::RecompileShaders()
{
	std::cout << "Debug mode detected, recompiling all shaders found in directory \"shaders\"...\n";
	auto oldPath = std::filesystem::current_path();
	auto newPath = std::filesystem::absolute("shaders");
	for (const auto& file : std::filesystem::directory_iterator(newPath))
	{
		if (file.path().extension() != ".bat")
			continue;
		std::string shaderComp = file.path().string();
		std::cout << "Executing script " << file.path().relative_path() << " for recompiling\n";
		std::filesystem::current_path(newPath);
		system(shaderComp.c_str()); // windows only!!
	}
	std::filesystem::current_path(oldPath);
}

void Renderer::InitVulkan()
{
	#ifdef _DEBUG // recompiles all the shaders with their .bat files, this simply makes it less of a hassle to change the shaders
	RecompileShaders();
	#endif

	instance = Vulkan::GenerateInstance();
	surface = Surface::GenerateSurface(instance, testWindow);
	physicalDevice = Vulkan::GetBestPhysicalDevice(instance, surface);
	SetLogicalDevice();
	swapchain = new Swapchain(logicalDevice, physicalDevice, surface, testWindow);
	swapchain->CreateImageViews();
	CreateDescriptorSetLayout();
	CreateGraphicsPipeline();
	CreateCommandPool();
	swapchain->CreateDepthBuffers();
	swapchain->CreateFramebuffers(renderPass);
	CreateDeferredFramebuffer(swapchain->extent.width, swapchain->extent.height);
	CreateIndirectDrawParametersBuffer();
	Texture::GeneratePlaceholderTextures();
	Mesh::materials.push_back({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, Texture::placeholderRoughness, Texture::placeholderAmbientOcclusion });
	if (defaultSampler == VK_NULL_HANDLE)
		CreateTextureSampler();
	CreateUniformBuffers();
	CreateModelDataBuffers();
	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffer();
	CreateSyncObjects();
	CreateImGUI();
	
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (!initGlobalBuffers)
	{
		globalVertexBuffer.Reserve(100000, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		globalIndicesBuffer.Reserve(100000, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		initGlobalBuffers = true;
	}

	rayTracer = RayTracing::Create(testWindow, swapchain);
}

void Renderer::CreateIndirectDrawParametersBuffer()
{
	indirectDrawParameters.Reserve(MAX_MESHES, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
}

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

	VkResult result = vkCreateSampler(logicalDevice, &createInfo, nullptr, &defaultSampler);
	CheckVulkanResult("Failed to create the texture sampler", result, vkCreateSampler);
}

void Renderer::CreateModelDataBuffers()
{
	VkDeviceSize size = sizeof(glm::mat4) * MAX_MESHES;
	
	modelBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	modelBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	modelBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, modelBuffers[i], modelBuffersMemory[i]);
		vkMapMemory(logicalDevice, modelBuffersMemory[i], 0, size, 0, &modelBuffersMapped[i]);
	}
}

void Renderer::SetModelData(uint32_t currentImage, std::vector<Object*> objects)
{
	std::vector<ModelData> modelMatrices;
	for (Object* object : objects)
	{
		if (object->state != OBJECT_STATE_VISIBLE || !object->HasFinishedLoading())
			continue;

		ModelData data = { object->transform.GetModelMatrix(), glm::vec4(ResourceManager::ConvertHandleToVec3(object->handle), 1) };
		for (Mesh& mesh : object->meshes)
			modelMatrices.push_back(data);
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
		Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
		vkMapMemory(logicalDevice, uniformBuffersMemory[i], 0, size, 0, &uniformBuffersMapped[i]);
	}
}

void Renderer::CreateRenderPass()
{
	// swapchain renderpass

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = swapchain->format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;//VK_ATTACHMENT_LOAD_OP_DONT_CARE;
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
	CheckVulkanResult("Failed to create a render pass", result, vkCreateRenderPass);

	// deferred renderpass

	std::array<VkAttachmentDescription, 4> deferredAttachments{};
	for (int i = 0; i < deferredAttachments.size() - 1; i++) // first 3 are color attachments
	{
		deferredAttachments[i].format = VK_FORMAT_R8G8B8A8_SRGB;
		deferredAttachments[i].samples = VK_SAMPLE_COUNT_1_BIT;
		deferredAttachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		deferredAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		deferredAttachments[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		deferredAttachments[i].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	deferredAttachments[3].format = physicalDevice.GetDepthFormat(); // last attachment is depth buffer
	deferredAttachments[3].samples = VK_SAMPLE_COUNT_1_BIT;
	deferredAttachments[3].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	deferredAttachments[3].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	deferredAttachments[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	deferredAttachments[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	deferredAttachments[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	deferredAttachments[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	std::array<VkAttachmentReference, 3> deferredColorReferences{};
	for (int i = 0; i < deferredColorReferences.size(); i++)
	{
		deferredColorReferences[i].attachment = i;
		deferredColorReferences[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	}
	
	VkAttachmentReference deferredDepthReference{};
	deferredDepthReference.attachment = 3;
	deferredDepthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription deferredSubpass{};
	deferredSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	deferredSubpass.colorAttachmentCount = 3;
	deferredSubpass.pColorAttachments = deferredColorReferences.data();
	deferredSubpass.pDepthStencilAttachment = &deferredDepthReference;

	VkSubpassDependency deferredDependency{};
	deferredDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	deferredDependency.dstSubpass = 0;
	deferredDependency.srcAccessMask = 0;
	deferredDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	deferredDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	deferredDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	
	VkRenderPassCreateInfo deferredRenderPassInfo{};
	deferredRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	deferredRenderPassInfo.attachmentCount = static_cast<uint32_t>(deferredAttachments.size());
	deferredRenderPassInfo.pAttachments = deferredAttachments.data();
	deferredRenderPassInfo.subpassCount = 1;
	deferredRenderPassInfo.pSubpasses = &deferredSubpass;
	deferredRenderPassInfo.dependencyCount = 1;
	deferredRenderPassInfo.pDependencies = &deferredDependency;

	result = vkCreateRenderPass(logicalDevice, &deferredRenderPassInfo, nullptr, &deferredRenderPass);
	CheckVulkanResult("Failed to create the deferred render pass", result, vkCreateRenderPass);
}

void Renderer::CreateDeferredFramebuffer(uint32_t width, uint32_t height)
{
	if (!gBufferViews.empty())
	{
		vkDestroyFramebuffer(logicalDevice, deferredFramebuffer, nullptr);
		for (int i = 0; i < gBufferViews.size(); i++)
		{
			vkDestroyImage(logicalDevice, gBufferImages[i], nullptr);
			vkDestroyImageView(logicalDevice, gBufferViews[i], nullptr);
			vkFreeMemory(logicalDevice, gBufferMemories[i], nullptr);
		}
		gBufferImages.clear();
		gBufferViews.clear();
		gBufferMemories.clear();
	}

	gBufferImages.resize(3);
	gBufferViews.resize(3);
	gBufferMemories.resize(3);
	for (int i = 0; i < gBufferViews.size(); i++)
	{
		Vulkan::CreateImage(width, height, 1, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, gBufferImages[i], gBufferMemories[i]);
		gBufferViews[i] = Vulkan::CreateImageView(gBufferImages[i], VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	VkFormat depthFormat = physicalDevice.GetDepthFormat();
	Vulkan::CreateImage(width, height, 1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, deferredDepth, deferredDepthMemory);
	deferredDepthView = Vulkan::CreateImageView(deferredDepth, VK_IMAGE_VIEW_TYPE_2D, 1, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	std::array<VkImageView, 4> framebufferViews =
	{
		gBufferViews[0],
		gBufferViews[1],
		gBufferViews[2],
		deferredDepthView
	};

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.renderPass = deferredRenderPass;
	createInfo.attachmentCount = static_cast<uint32_t>(framebufferViews.size());
	createInfo.pAttachments = framebufferViews.data();
	createInfo.width = static_cast<uint32_t>(width);
	createInfo.height = static_cast<uint32_t>(height);
	createInfo.layers = 1;

	VkResult result = vkCreateFramebuffer(logicalDevice, &createInfo, nullptr, &deferredFramebuffer);
	CheckVulkanResult("Failed to create the deferred framebuffer", result, vkCreateFramebuffer);
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
	allocateInfo.pNext = nullptr;// &countAllocateInfo;

	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

	VkResult result = vkAllocateDescriptorSets(logicalDevice, &allocateInfo, descriptorSets.data());
	CheckVulkanResult("Failed to allocate the descriptor sets", result, vkAllocateDescriptorSets);

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
		imageInfo.sampler = defaultSampler;

		std::vector<VkDescriptorImageInfo> imageInfos(Renderer::MAX_BINDLESS_TEXTURES, imageInfo);

		std::array<VkWriteDescriptorSet, 2> writeDescriptorSets{};
		writeDescriptorSets[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSets[0].dstSet = descriptorSets[i];
		writeDescriptorSets[0].pBufferInfo = &bufferInfo;
		writeDescriptorSets[0].dstBinding = 0;
		writeDescriptorSets[0].descriptorCount = 1;

		writeDescriptorSets[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSets[1].dstSet = descriptorSets[i];
		writeDescriptorSets[1].pBufferInfo = &modelBufferInfo;
		writeDescriptorSets[1].dstBinding = 1;
		writeDescriptorSets[1].descriptorCount = 1;

		vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void Renderer::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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
	CheckVulkanResult("Failed to create the descriptor pool", result, vkCreateDescriptorPool);
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
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	modelLayoutBinding.descriptorCount = 1;
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	modelLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 2;
	samplerLayoutBinding.descriptorCount = MAX_FRAMES_IN_FLIGHT;//MAX_BINDLESS_TEXTURES;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 3> bindings = { layoutBinding, modelLayoutBinding, samplerLayoutBinding };
	std::vector<VkDescriptorBindingFlags> bindingFlags = { 0, 0, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT };

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
	CheckVulkanResult("Failed to create the descriptor set layout", result, vkCreateDescriptorSetLayout);
}

void Renderer::CreateGraphicsPipeline()
{
	//create pipeline layout
	VkPipelineDynamicStateCreateInfo dynamicState = Vulkan::GetDynamicStateCreateInfo(dynamicStates);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = false;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthBoundsTestEnable = VK_FALSE;

	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineViewportStateCreateInfo viewportState = Vulkan::GetDefaultViewportStateCreateInfo(viewport, scissor, swapchain->extent);

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.lineWidth = 2;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.blendEnable = VK_TRUE;

	VkPipelineColorBlendStateCreateInfo blendCreateInfo{};
	blendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	blendCreateInfo.logicOpEnable = VK_FALSE;
	blendCreateInfo.pAttachments = &blendAttachment;
	blendCreateInfo.attachmentCount = 1;

	VkPushConstantRange pushConstant{};
	pushConstant.offset = 0;
	pushConstant.size = sizeof(glm::mat4);
	pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &descriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstant;

	VkResult result = vkCreatePipelineLayout(logicalDevice, &layoutInfo, nullptr, &pipelineLayout);
	CheckVulkanResult("Failed to create a pipeline layout", result, vkCreatePipelineLayout);
		
	CreateRenderPass();

	// world shaders pipeline

	VkShaderModule vertexShaderModule = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/vert.spv"));
	VkShaderModule fragmentShaderModule = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/frag.spv"));

	VkPipelineShaderStageCreateInfo vertexCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(vertexShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
	VkPipelineShaderStageCreateInfo fragmentCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(fragmentShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);
	VkPipelineShaderStageCreateInfo shaderStages[] = { vertexCreateInfo, fragmentCreateInfo };

	VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState =   &vertexInputInfo;
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState =      &viewportState;
	pipelineCreateInfo.pRasterizationState = &rasterizer;
	pipelineCreateInfo.pMultisampleState =   &multisampling;
	pipelineCreateInfo.pColorBlendState =    &blendCreateInfo;
	pipelineCreateInfo.pDynamicState =       &dynamicState;
	pipelineCreateInfo.pDepthStencilState =  &depthStencil;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;

	// screen shaders pipeline

	VkShaderModule screenShaderVert = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/screen.vert.spv"));
	VkShaderModule screenShaderFrag = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/screen.frag.spv"));

	vertexCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(screenShaderVert, VK_SHADER_STAGE_VERTEX_BIT);
	fragmentCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(screenShaderFrag, VK_SHADER_STAGE_FRAGMENT_BIT);
	VkPipelineShaderStageCreateInfo screenShaderStages[] = { vertexCreateInfo, fragmentCreateInfo };
	
	VkGraphicsPipelineCreateInfo screenPipelineInfo = pipelineCreateInfo;
	screenPipelineInfo.pStages = screenShaderStages;
	VkGraphicsPipelineCreateInfo createInfos[] = { pipelineCreateInfo, screenPipelineInfo };

	result = vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 2, createInfos, nullptr, &graphicsPipeline);
	CheckVulkanResult("Failed to create a graphics pipeline", result, vkCreateGraphicsPipelines);

	vkDestroyShaderModule(logicalDevice, screenShaderVert, nullptr);
	vkDestroyShaderModule(logicalDevice, screenShaderFrag, nullptr);

	vkDestroyShaderModule(logicalDevice, vertexShaderModule, nullptr);
	vkDestroyShaderModule(logicalDevice, fragmentShaderModule, nullptr);
}

void Renderer::SetLogicalDevice()
{
	logicalDevice = physicalDevice.GetLogicalDevice(surface);
	QueueFamilyIndices indices = physicalDevice.QueueFamilies(surface);
	queueIndex = indices.graphicsFamily.value();

	vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
	vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
	
	Vulkan::InitializeContext({ instance, logicalDevice, physicalDevice, graphicsQueue, queueIndex });
}

void Renderer::CreateCommandPool()
{
	commandPool = Vulkan::FetchNewCommandPool(queueIndex);
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
	CheckVulkanResult("Failed to allocate the command buffer", result, vkAllocateCommandBuffers);
}

void Renderer::SetViewport(VkCommandBuffer commandBuffer)
{
	VkViewport viewport{};
	Vulkan::PopulateDefaultViewport(viewport, swapchain->extent);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
}

void Renderer::SetScissors(VkCommandBuffer commandBuffer)
{
	VkRect2D scissor{};
	Vulkan::PopulateDefaultScissors(scissor, swapchain->extent);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Renderer::RecordCommandBuffer(VkCommandBuffer lCommandBuffer, uint32_t imageIndex, std::vector<Object*> objects, Camera* camera)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	VkResult result = vkBeginCommandBuffer(lCommandBuffer, &beginInfo);
	CheckVulkanResult("Failed to begin the given command buffer", result, nameof(vkBeginCommandBuffer));

	rayTracer->DrawFrame(objects, testWindow, camera, viewportWidth, viewportHeight, lCommandBuffer, imageIndex);

	if (denoiseOutput)
	{
		rayTracer->PrepareForDenoising(lCommandBuffer);
		result = vkEndCommandBuffer(lCommandBuffer);
		CheckVulkanResult("Failed to end the given command buffer", result, nameof(vkEndCommandBuffer));

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &lCommandBuffer;

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

		result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
		CheckVulkanResult("Failed to submit the queue", result, nameof(vkQueueSubmit));
		
		cudaExternalSemaphoreWaitParams waitParams{};
		memset(&waitParams, 0, sizeof(waitParams));
		waitParams.flags = 0;
		waitParams.params.fence.value = 1;
		
		CheckCudaResult(cudaWaitExternalSemaphoresAsync(&externalRenderSemaphores[currentFrame], &waitParams, 1, nullptr));

		cudaExternalSemaphoreSignalParams signalParams{};
		memset(&signalParams, 0, sizeof(signalParams));
		signalParams.flags = 0;
		signalParams.params.fence.value = 0;

		CheckCudaResult(cudaSignalExternalSemaphoresAsync(&externalRenderSemaphores[currentFrame], &signalParams, 1, nullptr));

		//vkDeviceWaitIdle(logicalDevice);

		rayTracer->DenoiseImage();
		cuStreamSynchronize(nullptr);

		vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
		vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

		vkResetCommandBuffer(commandBuffers[currentFrame], 0);
		VkResult result = vkBeginCommandBuffer(lCommandBuffer, &beginInfo);
		CheckVulkanResult("Failed to begin the given command buffer", result, nameof(vkBeginCommandBuffer));

		rayTracer->ApplyDenoisedImage(lCommandBuffer);
	}

	if (shouldRasterize)
	{
		std::array<VkClearValue, 4> deferredClearColors{};
		deferredClearColors[0].color = { 0, 0, 0, 1 };
		deferredClearColors[1].color = { 0, 0, 0, 1 };
		deferredClearColors[2].color = { 1, 1, 1, 1 };
		deferredClearColors[3].depthStencil = { 1, 0 };

		VkRenderPassBeginInfo deferredRenderPassBeginInfo{};
		deferredRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		deferredRenderPassBeginInfo.renderPass = deferredRenderPass;
		deferredRenderPassBeginInfo.framebuffer = deferredFramebuffer;
		deferredRenderPassBeginInfo.renderArea.offset = { 0, 0 };
		deferredRenderPassBeginInfo.renderArea.extent = swapchain->extent;
		deferredRenderPassBeginInfo.clearValueCount = static_cast<uint32_t>(deferredClearColors.size());
		deferredRenderPassBeginInfo.pClearValues = deferredClearColors.data();

		vkCmdBeginRenderPass(lCommandBuffer, &deferredRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(lCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		SetViewport(lCommandBuffer);
		SetScissors(lCommandBuffer);

		VkBuffer vertexBuffers[] = { globalVertexBuffer.GetBufferHandle() };
		VkDeviceSize offsets[] = { 0 };

		vkCmdBindDescriptorSets(lCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
		vkCmdBindVertexBuffers(lCommandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(lCommandBuffer, globalIndicesBuffer.GetBufferHandle(), 0, VK_INDEX_TYPE_UINT16);
 
		vkCmdDrawIndexedIndirect(lCommandBuffer, indirectDrawParameters.GetBufferHandle(), 0, (uint32_t)indirectDrawParameters.GetSize(), sizeof(VkDrawIndexedIndirectCommand));
		vkCmdEndRenderPass(lCommandBuffer);
	}

	VkImageView imageToCopy = rayTracer->gBufferViews[0];
	if (RayTracing::showNormals)
		imageToCopy = rayTracer->gBufferViews[2];
	else if (RayTracing::showAlbedo)
		imageToCopy = rayTracer->gBufferViews[1];

	std::array<VkClearValue, 2> clearColors{};
	clearColors[0].color = { 0, 0, 0, 1 };
	clearColors[1].depthStencil = { 1, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = swapchain->framebuffers[imageIndex];
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = swapchain->extent;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
	renderPassBeginInfo.pClearValues = clearColors.data();

	vkCmdBeginRenderPass(lCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	UpdateScreenShaderTexture(currentFrame, imageToCopy);

	SetViewport(lCommandBuffer);
	SetScissors(lCommandBuffer);

	if (shouldRenderCollisionBoxes)
	{
		vkCmdBindDescriptorSets(lCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
		vkCmdBindPipeline(lCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		RenderCollisionBoxes(objects, lCommandBuffer, currentFrame);
	}

	glm::vec4 offsets = glm::vec4(viewportOffsets.x, viewportOffsets.y, 1, 1);
	vkCmdBindDescriptorSets(lCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
	vkCmdBindPipeline(lCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screenPipeline);
	vkCmdPushConstants(lCommandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4), &offsets);
	vkCmdDraw(lCommandBuffer, 6, 1, 0, 0);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), lCommandBuffer);
	vkCmdEndRenderPass(lCommandBuffer);

	result = vkEndCommandBuffer(lCommandBuffer);
	CheckVulkanResult("Failed to record / end the command buffer", result, nameof(vkEndCommandBuffer));
}

void Renderer::CreateSyncObjects()
{
	imageAvaibleSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	externalRenderSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	externalRenderSemaphoreHandles.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkExportSemaphoreCreateInfo semaphoreExportInfo{};
	semaphoreExportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
	semaphoreExportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = &semaphoreExportInfo;

	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	VkSemaphoreGetWin32HandleInfoKHR getHandleInfo{};
	getHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
	getHandleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvaibleSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(logicalDevice, &fenceCreateInfo, nullptr, &inFlightFences[i]))
			throw VulkanAPIError("Failed to create the required semaphores and fence", VK_SUCCESS, nameof(CreateSyncObjects), __FILENAME__, __LINE__); // too difficult / annoying to put all of these calls into result = ...
	
		getHandleInfo.semaphore = renderFinishedSemaphores[i];

		VkResult result = vkGetSemaphoreWin32HandleKHR(logicalDevice, &getHandleInfo, &externalRenderSemaphoreHandles[i]);
		CheckVulkanResult("Failed to get the win32 handle of a semaphore", result, vkGetSemaphoreWin32HandleKHR);

		cudaExternalSemaphoreHandleDesc externSemaphoreDesc{};
		externSemaphoreDesc.type = cudaExternalSemaphoreHandleTypeOpaqueWin32;
		externSemaphoreDesc.handle.win32.handle = externalRenderSemaphoreHandles[i];
		externSemaphoreDesc.flags = 0;

		cudaError_t cuResult = cudaImportExternalSemaphore(&externalRenderSemaphores[i], &externSemaphoreDesc);
	}
}

void Renderer::RenderIntro(Intro* intro)
{
	std::chrono::steady_clock::time_point beginTime = std::chrono::high_resolution_clock::now();
	float currentTime = 0;

	while (currentTime < Intro::maxSeconds)
	{
		vkWaitForFences(logicalDevice, 1, &inFlightFences[0], true, UINT64_MAX);

		uint32_t imageIndex = GetNextSwapchainImage(0);

		vkResetFences(logicalDevice, 1, &inFlightFences[0]);
		vkResetCommandBuffer(commandBuffers[0], 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkResult result = vkBeginCommandBuffer(commandBuffers[0], &beginInfo);
		CheckVulkanResult("Failed to begin the given command buffer", result, nameof(vkBeginCommandBuffer));

		intro->WriteDataToBuffer(currentTime);
		intro->RecordCommandBuffer(commandBuffers[0], imageIndex);

		result = vkEndCommandBuffer(commandBuffers[0]);
		CheckVulkanResult("Failed to record / end the command buffer", result, nameof(vkEndCommandBuffer));

		SubmitRenderingCommandBuffer(0, imageIndex);
		PresentSwapchainImage(0, imageIndex);

		Win32Window::PollMessages();

		currentTime = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - beginTime).count(); // get the time that elapsed from the beginning till now
	}
}

void Renderer::OnResize()
{
	swapchain->Recreate(renderPass);
	rayTracer->RecreateImage(testWindow->GetWidth() * internalScale, testWindow->GetHeight() * internalScale);
	CreateDeferredFramebuffer(swapchain->extent.width * internalScale, swapchain->extent.height * internalScale);
	UpdateScreenShaderTexture(currentFrame);
	viewportWidth = testWindow->GetWidth() * viewportTransModifiers.x * internalScale;
	viewportHeight = testWindow->GetHeight() * viewportTransModifiers.y * internalScale;
	testWindow->resized = false;
	Console::WriteLine("Resized to " + std::to_string(testWindow->GetWidth()) + 'x' + std::to_string(testWindow->GetHeight()) + " px");
}

uint32_t Renderer::GetNextSwapchainImage(uint32_t frameIndex)
{
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(logicalDevice, swapchain->vkSwapchain, UINT64_MAX, imageAvaibleSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		OnResize();
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		throw VulkanAPIError("Failed to acquire the next swap chain image", result, nameof(vkAcquireNextImageKHR), __FILENAME__, __LINE__);
	return imageIndex;
}

void Renderer::PresentSwapchainImage(uint32_t frameIndex, uint32_t imageIndex)
{
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[frameIndex];

	VkSwapchainKHR swapchains[] = { swapchain->vkSwapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchains;
	presentInfo.pImageIndices = &imageIndex;

	VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || testWindow->resized)
	{
		OnResize();
	}
	else CheckVulkanResult("Failed to present the swap chain image", result, nameof(vkQueuePresentKHR));
}

void Renderer::SubmitRenderingCommandBuffer(uint32_t frameIndex, uint32_t imageIndex)
{
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvaibleSemaphores[frameIndex] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[frameIndex];

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[frameIndex] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	LockLogicalDevice(logicalDevice);
	std::lock_guard<std::mutex> lockGuard(Vulkan::graphicsQueueMutex);
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[frameIndex]);
	CheckVulkanResult("Failed to submit the queue", result, nameof(vkQueueSubmit));
}

void Renderer::DrawFrame(const std::vector<Object*>& objects, Camera* camera, float delta)
{
	std::vector<Object*> activeObjects;
	for (Object* object : objects)
		if (object->HasFinishedLoading() && object->state == OBJECT_STATE_VISIBLE && !object->meshes.empty())
			activeObjects.push_back(object);
	
	ImGui::Render();

	if (activeObjects.empty())
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		return;
	}
		
	std::lock_guard<std::mutex> lockGuard(drawingMutex);
	
	vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], true, UINT64_MAX);
	vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
	
	uint32_t imageIndex = GetNextSwapchainImage(currentFrame);

	UpdateUniformBuffers(currentFrame, camera);
	SetModelData(currentFrame, activeObjects);
	WriteIndirectDrawParameters(activeObjects);

	memcpy(&Renderer::selectedHandle, rayTracer->handleBufferMemPointer, sizeof(uint64_t));
	memset(rayTracer->handleBufferMemPointer, 0, sizeof(uint64_t));

	//UpdateBindlessTextures(currentFrame, activeObjects);

	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	RecordCommandBuffer(commandBuffers[currentFrame], imageIndex, activeObjects, camera);

	SubmitRenderingCommandBuffer(currentFrame, imageIndex);

	PresentSwapchainImage(currentFrame, imageIndex);

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	
	ImGui_ImplVulkan_NewFrame(); // not great that this is mentionned twice in one function
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void Renderer::WriteIndirectDrawParameters(std::vector<Object*>& objects)
{
	indirectDrawParameters.ResetAddressPointer();
	std::vector<VkDrawIndexedIndirectCommand> parameters;
	for (int i = 0; i < objects.size(); i++)
	{
		for (int j = 0; j < objects[i]->meshes.size(); j++)
		{
			VkDrawIndexedIndirectCommand parameter{};
			parameter.indexCount = static_cast<uint32_t>(objects[i]->meshes[j].indices.size());
			parameter.firstIndex = static_cast<uint32_t>(globalIndicesBuffer.GetItemOffset(objects[i]->meshes[j].indexMemory));
			parameter.vertexOffset = static_cast<uint32_t>(globalVertexBuffer.GetItemOffset(objects[i]->meshes[j].vertexMemory));
			parameter.instanceCount = 1;

			parameters.push_back(parameter);
		}
	}
	indirectDrawParameters.SubmitNewData(parameters);
}

void Renderer::UpdateScreenShaderTexture(uint32_t currentFrame, VkImageView imageView)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = imageView == VK_NULL_HANDLE ? rayTracer->gBufferViews[0] : imageView;
	imageInfo.sampler = defaultSampler;

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.pImageInfo = &imageInfo;
	writeSet.dstSet = descriptorSets[currentFrame];
	writeSet.dstBinding = 2;
	writeSet.descriptorCount = 1;
	writeSet.dstArrayElement = 0;

	vkUpdateDescriptorSets(logicalDevice, 1, &writeSet, 0, nullptr);
}

std::vector<Object*> processedObjects;
void Renderer::UpdateBindlessTextures(uint32_t currentFrame, const std::vector<Object*>& objects)
{
	std::vector<VkDescriptorImageInfo> imageInfos(Mesh::materials.size() * deferredMaterialTextures.size()); // this needs to prematurely create all the image infos, beacause otherwise the pImageInfo for the write sets won't work
	std::vector<VkWriteDescriptorSet> writeSets;
	for (int i = 0; i < Mesh::materials.size(); i++)
	{
		if (processedMaterials.count(i) > 0 && processedMaterials[i] == Mesh::materials[i].handle)
			continue;

		for (int j = 0; j < deferredMaterialTextures.size(); j++)
		{
			uint32_t index = static_cast<uint32_t>(deferredMaterialTextures.size()) * i + j;

			VkDescriptorImageInfo& imageInfo = imageInfos[index];
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = Mesh::materials[i][deferredMaterialTextures[j]]->imageView;
			imageInfo.sampler = defaultSampler;

			VkWriteDescriptorSet writeSet{};
			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeSet.pImageInfo = &imageInfos[index];
			writeSet.dstSet = descriptorSets[currentFrame];
			writeSet.dstArrayElement = index;
			writeSet.dstBinding = 2;
			writeSet.descriptorCount = 1;
			writeSets.push_back(writeSet);
		}
		processedMaterials[i] = Mesh::materials[i].handle;
	}
	if (!writeSets.empty())
		vkUpdateDescriptorSets(logicalDevice, (uint32_t)writeSets.size(), writeSets.data(), 0, nullptr);
}

void Renderer::RenderCollisionBoxes(std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage)
{
	for (Object* object : objects)
	{
		if (object->rigid.shape.type != SHAPE_TYPE_BOX)
			continue;

		glm::mat4 localRotationModel = glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.x), glm::vec3(1, 0, 0)) * glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.y), glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.z), glm::vec3(0, 0, 1));
		glm::mat4 scaleModel = glm::scale(glm::identity<glm::mat4>(), object->rigid.shape.data);
		glm::mat4 translationModel = glm::translate(glm::identity<glm::mat4>(), object->transform.position);
		glm::mat4 trans = translationModel * localRotationModel * scaleModel;
		
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &trans);
		vkCmdDraw(commandBuffer, 36, 1, 0, 0);
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

void Renderer::SetViewportOffsets(glm::vec2 offsets)
{
	viewportOffsets = offsets;
}

void Renderer::SetViewportModifiers(glm::vec2 modifiers)
{
	viewportTransModifiers = modifiers;
}