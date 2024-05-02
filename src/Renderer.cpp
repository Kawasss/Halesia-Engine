#include <stdexcept>
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
#include "renderer/AnimationManager.h"
#include "renderer/AccelerationStructures.h"
#include "renderer/PipelineCreator.h"
#include "renderer/ForwardPlus.h"
#include "renderer/ComputeShader.h"

#include "system/Window.h"

#include "core/Console.h"
#include "core/Object.h"
#include "core/Camera.h"
#include "core/Behavior.h"

#define IMGUI_IMPLEMENTATION
#define IMGUI_DEFINE_MATH_OPERATORS
#include "implot.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_win32.h"

#include "renderer/Renderer.h"

#include "tools/common.h"

#define CheckCudaResult(result) if (result != cudaSuccess) throw std::runtime_error(std::to_string(result) + " at line " + std::to_string(__LINE__) + " in " + (std::string)__FILENAME__);

StorageBuffer<Vertex>   Renderer::g_vertexBuffer;
StorageBuffer<uint16_t> Renderer::g_indexBuffer;
StorageBuffer<Vertex>   Renderer::g_defaultVertexBuffer;

VkSampler Renderer::defaultSampler = VK_NULL_HANDLE;
Handle    Renderer::selectedHandle = 0;
bool      Renderer::initGlobalBuffers = false;
bool      Renderer::shouldRenderCollisionBoxes = false;
bool      Renderer::denoiseOutput = true;
bool      Renderer::canRayTrace = false;
float     Renderer::internalScale = 1;

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
	uint32_t width;
	uint32_t height;
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

	delete fwdPlus;

	animationManager->Destroy();
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

	vkDestroyQueryPool(logicalDevice, queryPool, nullptr);
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
	if (Behavior::arguments.find("-no_shader_recompilation") != Behavior::arguments.end())
	{
		Console::WriteLine("Cannot recompile shaders because of the argument \"-no_shader_recompilation\"");
		return;
	}

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
	canRayTrace = Vulkan::LogicalDeviceExtensionIsSupported(physicalDevice, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
	if (!canRayTrace)
		shouldRasterize = true;//throw VulkanAPIError("Cannot initialize the renderer: a required extension is not supported (Vulkan::LogicalDeviceExtensionIsSupported(physicalDevice, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
	else
	{
		Vulkan::requiredLogicalDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
		Vulkan::requiredLogicalDeviceExtensions.push_back(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
		Vulkan::requiredLogicalDeviceExtensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
		Vulkan::requiredLogicalDeviceExtensions.push_back(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
		Vulkan::requiredLogicalDeviceExtensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		Vulkan::requiredLogicalDeviceExtensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		Vulkan::requiredLogicalDeviceExtensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
	}

	SetLogicalDevice();
	DetectExternalTools();
	swapchain = new Swapchain(logicalDevice, physicalDevice, surface, testWindow);
	swapchain->CreateImageViews();
	queryPool = Vulkan::CreateQueryPool(VK_QUERY_TYPE_TIMESTAMP, 10);
	CreateDescriptorSetLayout();
	CreateGraphicsPipeline();
	
	CreateCommandPool();
	swapchain->CreateDepthBuffers();
	swapchain->CreateFramebuffers(renderPass);
	indirectDrawParameters.Reserve(MAX_MESHES, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	Texture::GeneratePlaceholderTextures();
	Mesh::materials.push_back({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, Texture::placeholderRoughness, Texture::placeholderAmbientOcclusion });
	if (defaultSampler == VK_NULL_HANDLE)
		CreateTextureSampler();

	fwdPlus = new ForwardPlusRenderer();
	for (int i = 0; i < 1; i++)
		fwdPlus->AddLight(glm::vec3(i / 16.0f, 0, 0)); // test !!

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
		g_defaultVertexBuffer.Reserve(1000000, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		g_vertexBuffer.Reserve(1000000, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		g_indexBuffer.Reserve(1000000, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		initGlobalBuffers = true;
	}
	animationManager = AnimationManager::Get();

	if (canRayTrace)
		rayTracer = RayTracing::Create(testWindow, swapchain);
	else shouldRasterize = true;
}

void Renderer::DetectExternalTools()
{
	uint32_t numTools = 0;
	vkGetPhysicalDeviceToolProperties(physicalDevice.Device(), &numTools, nullptr);
	std::cout << "Detected external tools:\n";
	if (numTools == 0)
	{
		std::cout << "  None\n\n";
		return;
	}

	std::vector<VkPhysicalDeviceToolProperties> properties(numTools);
	vkGetPhysicalDeviceToolProperties(physicalDevice.Device(), &numTools, properties.data());
	for (int i = 0; i < numTools; i++)
	{
		std::cout << 
			"\n  name: " << properties[i].name <<
			"\n  version: " << properties[i].version <<
			"\n  purposes: " << string_VkToolPurposeFlags(properties[i].purposes) <<
			"\n  description: " << properties[i].description << '\n';
	}
	std::cout << '\n';
}

void Renderer::SetInternalResolutionScale(float scale)
{
	internalScale = scale;
	OnResize();
}

float Renderer::GetInternalResolutionScale()
{
	return internalScale;
}

void Renderer::GetQueryResults()
{
	std::vector<uint64_t> results = Vulkan::GetQueryPoolResults(queryPool, 10);

	animationTime =           (results[1] - results[0]) * 0.000001f; // nanoseconds to milliseconds
	rebuildingTime =          (results[3] - results[2]) * 0.000001f;
	rayTracingTime =          (results[5] - results[4]) * 0.000001f;
	if (denoiseOutput)
	{
		denoisingPrepTime =   (results[7] - results[6]) * 0.000001f;
		finalRenderPassTime = (results[9] - results[8]) * 0.000001f;
	}
	else
	{
		finalRenderPassTime = (results[7] - results[6]) * 0.000001f;
		denoisingPrepTime = 0;
		denoisingTime = 0;
	}
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

void Renderer::SetModelData(uint32_t currentImage, const std::vector<Object*>& objects)
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

	renderPass = PipelineCreator::CreateRenderPass(physicalDevice, swapchain, PIPELINE_FLAG_CLEAR_ON_LOAD, 1);

	// deferred renderpass

	deferredRenderPass = PipelineCreator::CreateRenderPass(physicalDevice, swapchain, PIPELINE_FLAG_CLEAR_ON_LOAD, 1);
}

void Renderer::CreateDescriptorSets()
{
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

		VkDescriptorBufferInfo cellBufferInfo{};
		cellBufferInfo.buffer = fwdPlus->GetCellBuffer();
		cellBufferInfo.offset = 0;
		cellBufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = VK_NULL_HANDLE;
		imageInfo.sampler = defaultSampler;

		std::vector<VkDescriptorImageInfo> imageInfos(Renderer::MAX_BINDLESS_TEXTURES, imageInfo);

		std::array<VkWriteDescriptorSet, 3> writeDescriptorSets{};
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

		writeDescriptorSets[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSets[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writeDescriptorSets[2].dstSet = descriptorSets[i];
		writeDescriptorSets[2].pBufferInfo = &cellBufferInfo;
		writeDescriptorSets[2].dstBinding = 3;
		writeDescriptorSets[2].descriptorCount = 1;

		vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void Renderer::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[1].descriptorCount = MAX_FRAMES_IN_FLIGHT * 2;
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
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
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

	VkDescriptorSetLayoutBinding cellLayoutBinding{};
	cellLayoutBinding.binding = 3;
	cellLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cellLayoutBinding.descriptorCount = 1;
	cellLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	cellLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 4> bindings = { layoutBinding, modelLayoutBinding, samplerLayoutBinding, cellLayoutBinding };
	std::vector<VkDescriptorBindingFlags> bindingFlags = { 0, 0, VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, 0 };

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

	VkShaderModule vertexShaderModule = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/triangle.vert.spv"));
	VkShaderModule fragmentShaderModule = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/triangle.frag.spv"));

	VkPipelineShaderStageCreateInfo vertexCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(vertexShaderModule, VK_SHADER_STAGE_VERTEX_BIT);
	VkPipelineShaderStageCreateInfo fragmentCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(fragmentShaderModule, VK_SHADER_STAGE_FRAGMENT_BIT);

	graphicsPipeline = PipelineCreator::CreatePipeline(pipelineLayout, renderPass, swapchain, { vertexCreateInfo, fragmentCreateInfo }, PIPELINE_FLAG_CULL_BACK | PIPELINE_FLAG_FRONT_CCW);

	// screen shaders pipeline

	VkShaderModule screenShaderVert = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/screen.vert.spv"));
	VkShaderModule screenShaderFrag = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/screen.frag.spv"));

	vertexCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(screenShaderVert, VK_SHADER_STAGE_VERTEX_BIT);
	fragmentCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(screenShaderFrag, VK_SHADER_STAGE_FRAGMENT_BIT);

	screenPipeline = PipelineCreator::CreatePipeline(pipelineLayout, renderPass, swapchain, { vertexCreateInfo, fragmentCreateInfo }, PIPELINE_FLAG_CULL_BACK | PIPELINE_FLAG_FRONT_CCW | PIPELINE_FLAG_NO_VERTEX);

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
	vkGetDeviceQueue(logicalDevice, indices.computeFamily.value(), 0, &computeQueue);
	
	Vulkan::InitializeContext({ instance, logicalDevice, physicalDevice, graphicsQueue, queueIndex, presentQueue, indices.presentFamily.value(), computeQueue, indices.computeFamily.value() });
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

void Renderer::WriteTimestamp(VkCommandBuffer commandBuffer, bool reset)
{
	static uint32_t index = 0;
	vkCmdWriteTimestamp(commandBuffer, index % 2 == 0 ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, index);
	index = reset ? 0 : index + 1;
}

void Renderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, std::vector<Object*> objects, Camera* camera)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	
	VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	CheckVulkanResult("Failed to begin the given command buffer", result, vkBeginCommandBuffer);
	vkCmdResetQueryPool(commandBuffer, queryPool, 0, 10);

	WriteTimestamp(commandBuffer);
	//animationManager->ApplyAnimations(commandBuffer); // not good
	WriteTimestamp(commandBuffer);

	WriteTimestamp(commandBuffer);
	/*for (Object* obj : objects)
		for (Mesh& mesh : obj->meshes)
			mesh.BLAS->RebuildGeometry(commandBuffer, mesh);*/
	WriteTimestamp(commandBuffer);

	VkImageView imageToCopy = VK_NULL_HANDLE;
	if (canRayTrace)
	{
		WriteTimestamp(commandBuffer);
		if (!shouldRasterize)
			rayTracer->DrawFrame(objects, testWindow, camera, viewportWidth, viewportHeight, commandBuffer, imageIndex);

		WriteTimestamp(commandBuffer);
		if (denoiseOutput)
		{
			DenoiseSynchronized(commandBuffer);
		}

		imageToCopy = rayTracer->gBufferViews[0];
		if (RayTracing::showNormals)
			imageToCopy = rayTracer->gBufferViews[2];
		else if (RayTracing::showAlbedo)
			imageToCopy = rayTracer->gBufferViews[1];
	}

	if (shouldRasterize)
		fwdPlus->Draw(commandBuffer, camera); // should maybe (?) not be here when its fully implemented

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

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	if (canRayTrace)
		UpdateScreenShaderTexture(currentFrame, imageToCopy);

	WriteTimestamp(commandBuffer);

	SetViewport(commandBuffer);
	SetScissors(commandBuffer);

	if (shouldRenderCollisionBoxes)
	{
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		RenderCollisionBoxes(objects, commandBuffer, currentFrame);
	}

	if (shouldRasterize)
		RasterizeObjects(commandBuffer, objects);
	
	glm::vec4 offsets = glm::vec4(viewportOffsets.x, viewportOffsets.y, 1, 1);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screenPipeline);
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4), &offsets);
	if (!shouldRasterize) vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	vkCmdEndRenderPass(commandBuffer);

	WriteTimestamp(commandBuffer, true);

	result = vkEndCommandBuffer(commandBuffer);
	CheckVulkanResult("Failed to record / end the command buffer", result, nameof(vkEndCommandBuffer));
}

void Renderer::RasterizeObjects(VkCommandBuffer commandBuffer, const std::vector<Object*>& objects)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

	SetViewport(commandBuffer);
	SetScissors(commandBuffer);

	VkBuffer vertexBuffers[] = { g_vertexBuffer.GetBufferHandle() };
	VkDeviceSize offsets[] = { 0 };

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, g_indexBuffer.GetBufferHandle(), 0, VK_INDEX_TYPE_UINT16);

	vkCmdDrawIndexedIndirect(commandBuffer, indirectDrawParameters.GetBufferHandle(), 0, (uint32_t)indirectDrawParameters.GetSize(), sizeof(VkDrawIndexedIndirectCommand));
}

void Renderer::DenoiseSynchronized(VkCommandBuffer commandBuffer)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	WriteTimestamp(commandBuffer);
	rayTracer->PrepareForDenoising(commandBuffer);
	WriteTimestamp(commandBuffer);

	VkResult result = vkEndCommandBuffer(commandBuffer);
	CheckVulkanResult("Failed to end the given command buffer", result, vkEndCommandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

	result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
	CheckVulkanResult("Failed to submit the queue", result, vkQueueSubmit);

	submittedCount++;

	auto start = std::chrono::high_resolution_clock::now();
#ifdef USE_CUDA
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

	rayTracer->DenoiseImage();

	cuStreamSynchronize(nullptr);
#endif
	vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
	vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);

	denoisingTime = std::chrono::duration<float, std::chrono::milliseconds::period>(std::chrono::high_resolution_clock::now() - start).count();

	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	CheckVulkanResult("Failed to begin the given command buffer", result, vkBeginCommandBuffer);

	rayTracer->ApplyDenoisedImage(commandBuffer);
}

void Renderer::CreateSyncObjects()
{
	imageAvaibleSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkExportSemaphoreCreateInfo semaphoreExportInfo{};
	semaphoreExportInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
	semaphoreExportInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
	void* semaphoreNext = canRayTrace ? &semaphoreExportInfo : nullptr;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		imageAvaibleSemaphores[i] = Vulkan::CreateSemaphore(semaphoreNext);
		renderFinishedSemaphores[i] = Vulkan::CreateSemaphore(semaphoreNext);
		inFlightFences[i] = Vulkan::CreateFence(VK_FENCE_CREATE_SIGNALED_BIT);
	}

	if (canRayTrace)
		ExportSemaphores();
}

void Renderer::ExportSemaphores()
{
	externalRenderSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	externalRenderSemaphoreHandles.resize(MAX_FRAMES_IN_FLIGHT);

#ifdef USE_CUDA
	VkSemaphoreGetWin32HandleInfoKHR getHandleInfo{};
	getHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
	getHandleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		getHandleInfo.semaphore = renderFinishedSemaphores[i];

		VkResult result = vkGetSemaphoreWin32HandleKHR(logicalDevice, &getHandleInfo, &externalRenderSemaphoreHandles[i]);
		CheckVulkanResult("Failed to get the win32 handle of a semaphore", result, vkGetSemaphoreWin32HandleKHR);

		cudaExternalSemaphoreHandleDesc externSemaphoreDesc{};
		externSemaphoreDesc.type = cudaExternalSemaphoreHandleTypeOpaqueWin32;
		externSemaphoreDesc.handle.win32.handle = externalRenderSemaphoreHandles[i];
		externSemaphoreDesc.flags = 0;

		cudaError_t cuResult = cudaImportExternalSemaphore(&externalRenderSemaphores[i], &externSemaphoreDesc);
	}
#endif
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
	if (canRayTrace)
		rayTracer->RecreateImage(testWindow->GetWidth() * internalScale, testWindow->GetHeight() * internalScale);
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

	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain->vkSwapchain;
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

	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvaibleSemaphores[frameIndex];
	submitInfo.pWaitDstStageMask = &waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[frameIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[frameIndex];

	LockLogicalDevice(logicalDevice);
	std::lock_guard<std::mutex> lockGuard(Vulkan::graphicsQueueMutex);
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[frameIndex]);
	CheckVulkanResult("Failed to submit the queue", result, nameof(vkQueueSubmit));

	submittedCount++;
}

inline bool ObjectIsValid(Object* object)
{
	return object->HasFinishedLoading() && object->state == OBJECT_STATE_VISIBLE && !object->meshes.empty() && !object->shouldBeDestroyed;
}

inline void ResetImGui()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void Renderer::StartRecording()
{
	VkResult result = vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], true, UINT64_MAX);
	CheckVulkanResult("Failed to wait for fences", result, vkWaitForFences);
	result = vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
	CheckVulkanResult("Failed to reset fences", result, vkResetFences);

	Vulkan::DeleteSubmittedObjects();
	GetQueryResults();

	if (canRayTrace)
	{
		selectedHandle = *rayTracer->handleBufferMemPointer;
		*rayTracer->handleBufferMemPointer = 0;
	}

	imageIndex = GetNextSwapchainImage(currentFrame);

	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	submittedCount = 0;

	ImGui::Render();
}

void Renderer::SubmitRecording()
{
	SubmitRenderingCommandBuffer(currentFrame, imageIndex);
	PresentSwapchainImage(currentFrame, imageIndex);
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	ResetImGui();
}

inline void GetAllObjectsFromObject(std::vector<Object*>& ret, Object* obj)
{
	if (!ObjectIsValid(obj))
		return;

	ret.push_back(obj);
	for (Object* object : obj->children)
	{
		if (ObjectIsValid(object))
		{
			GetAllObjectsFromObject(ret, object);
		}
	}
}

void Renderer::RenderObjects(const std::vector<Object*>& objects, Camera* camera)
{
	std::vector<Object*> activeObjects;
	for (Object* object : objects)
	{
		GetAllObjectsFromObject(activeObjects, object);
	}

	receivedObjects += objects.size();
	renderedObjects += activeObjects.size();
	
	if (activeObjects.empty())
		return;

	std::lock_guard<std::mutex> lockGuard(drawingMutex);

	//UpdateBindlessTextures(currentFrame, activeObjects);
	UpdateUniformBuffers(currentFrame, camera);
	SetModelData(currentFrame, activeObjects);
	WriteIndirectDrawParameters(activeObjects);

	RecordCommandBuffer(commandBuffers[currentFrame], imageIndex, activeObjects, camera);
}

void Renderer::DrawFrame(const std::vector<Object*>& objects, Camera* camera, float delta)
{
	StartRecording();

	RenderObjects(objects, camera);

	SubmitRecording();
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
			parameter.firstIndex = static_cast<uint32_t>(g_indexBuffer.GetItemOffset(objects[i]->meshes[j].indexMemory));
			parameter.vertexOffset = static_cast<uint32_t>(g_vertexBuffer.GetItemOffset(objects[i]->meshes[j].vertexMemory));
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
	imageInfo.imageView = imageView == VK_NULL_HANDLE && canRayTrace ? rayTracer->gBufferViews[0] : imageView;
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

void Renderer::RenderCollisionBoxes(const std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage)
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
	ubo.width = testWindow->GetWidth();
	ubo.height = testWindow->GetHeight();
	memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Renderer::SetViewportOffsets(glm::vec2 offsets)
{
	viewportOffsets = offsets;
}

void Renderer::SetViewportModifiers(glm::vec2 modifiers)
{
	if (viewportTransModifiers == modifiers)
		return;
	viewportTransModifiers = modifiers;
	OnResize();
}