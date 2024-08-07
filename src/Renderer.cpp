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
#include "renderer/DescriptorWriter.h"
#include "renderer/RenderPipeline.h"

#include "system/Window.h"

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

#include "tools/common.h"

#define CheckCudaResult(result) if (result != cudaSuccess) throw std::runtime_error(std::to_string(result) + " at line " + std::to_string(__LINE__) + " in " + (std::string)__FILENAME__);

StorageBuffer<Vertex>   Renderer::g_vertexBuffer;
StorageBuffer<uint16_t> Renderer::g_indexBuffer;
StorageBuffer<Vertex>   Renderer::g_defaultVertexBuffer;

VkSampler Renderer::defaultSampler  = VK_NULL_HANDLE;
VkSampler Renderer::noFilterSampler = VK_NULL_HANDLE;
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

VkMemoryAllocateFlagsInfo allocateFlagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0 };

Renderer::Renderer(Window* window, RendererFlags flags)
{
	this->flags = flags;
	testWindow = window;
	window->additionalPollCallback = ImGui_ImplWin32_WndProcHandler;
	Vulkan::optionalMemoryAllocationFlags = &allocateFlagsInfo;
	InitVulkan();
}

void Renderer::Destroy()
{
	vkDeviceWaitIdle(logicalDevice);

	Vulkan::DeleteSubmittedObjects();
	Vulkan::DestroyAllCommandPools();

	for (Material& mat : Mesh::materials)
		mat.Destroy();

	indirectDrawParameters.Destroy();
	g_defaultVertexBuffer.Destroy();
	g_vertexBuffer.Destroy();
	g_indexBuffer.Destroy();

	queryPool.~QueryPool();

	for (RenderPipeline* renderPipeline : renderPipelines)
	{
		renderPipeline->Destroy();
		delete renderPipeline;
	}

	delete writer;

	delete animationManager;
	delete rayTracer;

	vkDestroyDescriptorPool(logicalDevice, imGUIDescriptorPool, nullptr);
	ImGui_ImplVulkan_Shutdown();

	delete swapchain;

	vkDestroySampler(logicalDevice, defaultSampler, nullptr);
	vkDestroySampler(logicalDevice, noFilterSampler, nullptr);

	vkDestroyImage(logicalDevice, resultImage, nullptr);
	vkDestroyImageView(logicalDevice, resultView, nullptr);
	vkFreeMemory(logicalDevice, resultMemory, nullptr);

	vkDestroyImage(logicalDevice, resultDepth, nullptr);
	vkDestroyImageView(logicalDevice, depthView, nullptr);
	vkFreeMemory(logicalDevice, depthMemory, nullptr);

	vkDestroyFramebuffer(logicalDevice, resultFramebuffer, nullptr);

	Texture::DestroyPlaceholderTextures();

	vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);

	vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);

	vkDestroyPipeline(logicalDevice, screenPipeline, nullptr);

	vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
	vkDestroyRenderPass(logicalDevice, GUIRenderPass, nullptr);

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
	if (flags & NO_SHADER_RECOMPILATION)
	{
		Console::WriteLine("Cannot recompile shaders because the flag NO_SHADER_RECOMPILATION was set");
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

	CreatePhysicalDevice();
	
	uint32_t numTools = DetectExternalTools();
	if (flags & NO_VALIDATION || numTools > 0)
	{
		const char* reason = numTools > 0 ? " due to possible interference of external tools\n" : ", the flag NO_VALIDATION was set\n";
		std::cout << "Disabled validation layers" << reason;
		Vulkan::DisableValidationLayers();
	}

	CreateContext();
	CreateCommandPool();

	swapchain = new Swapchain(surface, testWindow, false);
	swapchain->CreateImageViews();
	queryPool.Create(VK_QUERY_TYPE_TIMESTAMP, 10);
	CreateDescriptorSetLayout();
	CreateGraphicsPipeline();
	
	swapchain->CreateDepthBuffers();
	swapchain->CreateFramebuffers(renderPass);
	indirectDrawParameters.Reserve(MAX_MESHES, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT);
	Texture::GeneratePlaceholderTextures();
	Mesh::materials.push_back({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, Texture::placeholderRoughness, Texture::placeholderAmbientOcclusion });
	if (defaultSampler == VK_NULL_HANDLE)
		CreateTextureSampler();

	if (!initGlobalBuffers)
	{
		VkBufferUsageFlags commonFlags = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		g_defaultVertexBuffer.Reserve(1000000, commonFlags | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		g_vertexBuffer.Reserve(1000000,        commonFlags | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		g_indexBuffer.Reserve(1000000,         commonFlags | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
		initGlobalBuffers = true;
	}

	writer = DescriptorWriter::Get();
	animationManager = AnimationManager::Get();

	if (canRayTrace)
	{
		rayTracer = new RayTracingPipeline;
		rayTracer->renderPass = renderPass;
		rayTracer->Start(GetPipelinePayload(VK_NULL_HANDLE, nullptr));
	}
	else shouldRasterize = true;

	CreateDescriptorPool();
	CreateDescriptorSets();
	CreateCommandBuffer();
	CreateSyncObjects();
	CreateImGUI();
	
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	viewportWidth = testWindow->GetWidth() * internalScale;
	viewportHeight = testWindow->GetHeight() * internalScale;

	UpdateScreenShaderTexture(0);
}

void Renderer::CreateContext()
{
	CreatePhysicalDevice();

	AddExtensions();

	SetLogicalDevice();
}

void Renderer::CreatePhysicalDevice()
{
	#ifdef _DEBUG
	Vulkan::requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	#endif

	instance = Vulkan::GenerateInstance();

	Vulkan::ActiveInstanceExtensions(instance, Vulkan::requiredInstanceExtensions);

	surface = Surface::GenerateSurface(instance, testWindow);
	physicalDevice = Vulkan::GetBestPhysicalDevice(instance, surface);
}

void Renderer::AddExtensions()
{
	canRayTrace = Vulkan::LogicalDeviceExtensionIsSupported(physicalDevice, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) && !(flags & NO_RAY_TRACING);
	if (!canRayTrace)
		shouldRasterize = true;
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
}

uint32_t Renderer::DetectExternalTools()
{
	uint32_t numTools = 0;
	vkGetPhysicalDeviceToolProperties(physicalDevice.Device(), &numTools, nullptr);
	std::cout << "Detected external tools:\n";
	if (numTools == 0)
	{
		std::cout << "  None\n";
		return numTools;
	}

	std::vector<VkPhysicalDeviceToolProperties> properties(numTools);
	vkGetPhysicalDeviceToolProperties(physicalDevice.Device(), &numTools, properties.data());
	for (int i = 0; i < numTools; i++)
	{
		std::cout << 
			"\n  name: " << properties[i].name <<
			"\n  version: " << properties[i].version <<
			"\n  purposes: " << string_VkToolPurposeFlags(properties[i].purposes) <<
			"\n  description: " << properties[i].description << "\n";
	}
	return numTools;
}

void Renderer::CreateTextureSampler()
{
	VkPhysicalDeviceProperties deviceProperties = physicalDevice.Properties();

	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;
	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	createInfo.maxAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;
	createInfo.anisotropyEnable = VK_TRUE;
	createInfo.unnormalizedCoordinates = VK_FALSE;
	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias = 0;
	createInfo.minLod = 0;
	createInfo.maxLod = VK_LOD_CLAMP_NONE;//static_cast<uint32_t>(textureImage->GetMipLevels());

	VkResult result = vkCreateSampler(logicalDevice, &createInfo, nullptr, &defaultSampler);
	CheckVulkanResult("Failed to create the texture sampler", result, vkCreateSampler);

	createInfo.magFilter = createInfo.minFilter = VK_FILTER_NEAREST;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

	result = vkCreateSampler(logicalDevice, &createInfo, nullptr, &noFilterSampler);
	CheckVulkanResult("Failed to create the texture sampler", result, vkCreateSampler);

	resultSampler = flags & NO_FILTERING_ON_RESULT ? noFilterSampler : defaultSampler;
}

void Renderer::CreateRenderPass()
{
	renderPass    = PipelineCreator::CreateRenderPass(physicalDevice, swapchain->format, PIPELINE_FLAG_CLEAR_ON_LOAD, 1);
	GUIRenderPass = PipelineCreator::CreateRenderPass(physicalDevice, swapchain->format, PIPELINE_FLAG_NONE, 1);
}

void Renderer::CreateDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	allocateInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

	VkResult result = vkAllocateDescriptorSets(logicalDevice, &allocateInfo, descriptorSets.data());
	CheckVulkanResult("Failed to allocate the descriptor sets", result, vkAllocateDescriptorSets);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		writer->WriteImage(descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, canRayTrace ? rayTracer->gBufferViews[0] : VK_NULL_HANDLE, defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
}

void Renderer::CreateDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 1> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes = poolSizes.data();
	createInfo.maxSets = MAX_FRAMES_IN_FLIGHT;
	createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;

	VkResult result = vkCreateDescriptorPool(logicalDevice, &createInfo, nullptr, &descriptorPool);
	CheckVulkanResult("Failed to create the descriptor pool", result, vkCreateDescriptorPool);
}

void Renderer::CreateDescriptorSetLayout() // should simplify this with shader reflection
{
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 2;
	samplerLayoutBinding.descriptorCount = MAX_FRAMES_IN_FLIGHT;//MAX_BINDLESS_TEXTURES;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array<VkDescriptorSetLayoutBinding, 1> bindings = { samplerLayoutBinding };
	std::vector<VkDescriptorBindingFlags> bindingFlags = { VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT };

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

	// screen shaders pipeline

	VkShaderModule screenShaderVert = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/screen.vert.spv"));
	VkShaderModule screenShaderFrag = Vulkan::CreateShaderModule(ReadFile("shaders/spirv/screen.frag.spv"));

	VkPipelineShaderStageCreateInfo vertexCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(screenShaderVert, VK_SHADER_STAGE_VERTEX_BIT);
	VkPipelineShaderStageCreateInfo fragmentCreateInfo = Vulkan::GetGenericShaderStageCreateInfo(screenShaderFrag, VK_SHADER_STAGE_FRAGMENT_BIT);

	screenPipeline = PipelineCreator::CreatePipeline(pipelineLayout, renderPass, { vertexCreateInfo, fragmentCreateInfo }, PIPELINE_FLAG_CULL_BACK | PIPELINE_FLAG_FRONT_CCW | PIPELINE_FLAG_NO_VERTEX);

	vkDestroyShaderModule(logicalDevice, screenShaderVert, nullptr);
	vkDestroyShaderModule(logicalDevice, screenShaderFrag, nullptr);
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

void Renderer::ProcessRenderPipeline(RenderPipeline* pipeline)
{
	pipeline->renderPass = renderPass;
	pipeline->Start(GetPipelinePayload(VK_NULL_HANDLE, nullptr));
	renderPipelines.push_back(pipeline);
}

void Renderer::WriteTimestamp(VkCommandBuffer commandBuffer, bool reset)
{
	queryPool.WriteTimeStamp(commandBuffer);
}

void Renderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, std::vector<Object*> objects, Camera* camera)
{
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	
	VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	CheckVulkanResult("Failed to begin the given command buffer", result, vkBeginCommandBuffer);

	Vulkan::InsertDebugLabel(commandBuffer, "drawing buffer");

	queryPool.Reset(commandBuffer);

	WriteTimestamp(commandBuffer);
	//animationManager->ApplyAnimations(commandBuffer); // not good
	WriteTimestamp(commandBuffer);

	WriteTimestamp(commandBuffer);
	//for (Object* obj : objects)
	//	obj->mesh.BLAS->RebuildGeometry(commandBuffer, obj->mesh);
	WriteTimestamp(commandBuffer);

	VkImageView imageToCopy = VK_NULL_HANDLE;
	if (canRayTrace)
	{
		Vulkan::StartDebugLabel(commandBuffer, "ray tracing");

		WriteTimestamp(commandBuffer);
		if (!shouldRasterize)
			rayTracer->Execute(GetPipelinePayload(commandBuffer, camera), objects);

		WriteTimestamp(commandBuffer);
		if (denoiseOutput)
		{
			DenoiseSynchronized(commandBuffer);
		}

		imageToCopy = rayTracer->gBufferViews[0];
		if (RayTracingPipeline::showNormals)
			imageToCopy = rayTracer->gBufferViews[2];
		else if (RayTracingPipeline::showAlbedo)
			imageToCopy = rayTracer->gBufferViews[1];

		vkCmdEndDebugUtilsLabelEXT(commandBuffer);
	}

	VkExtent2D viewportExtent = { viewportWidth, viewportHeight };
	SetViewport(commandBuffer, viewportExtent);
	SetScissors(commandBuffer, viewportExtent);

	if (shouldRasterize)
	{
		RenderPipeline::Payload payload = GetPipelinePayload(commandBuffer, camera);
		for (RenderPipeline* renderPipeline : renderPipelines)
		{
			Vulkan::StartDebugLabel(commandBuffer, dbgPipelineNames[renderPipeline] + "::Execute");
			renderPipeline->Execute(payload, objects);
			vkCmdEndDebugUtilsLabelEXT(commandBuffer);
		}
	}

	Vulkan::StartDebugLabel(commandBuffer, "UI");

	SetViewport(commandBuffer, swapchain->extent);
	SetScissors(commandBuffer, swapchain->extent);

	std::array<VkClearValue, 2> clearColors{};
	clearColors[0].color = { 0, 0, 0, 1 };
	clearColors[1].depthStencil = { 1, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = GUIRenderPass;
	renderPassBeginInfo.framebuffer = swapchain->framebuffers[imageIndex];
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = swapchain->extent;
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
	renderPassBeginInfo.pClearValues = clearColors.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	WriteTimestamp(commandBuffer);

	glm::vec4 offsets = glm::vec4(viewportOffsets.x, viewportOffsets.y, 1, 1);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, screenPipeline);
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::vec4), &offsets);
	vkCmdDraw(commandBuffer, 6, 1, 0, 0);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	vkCmdEndRenderPass(commandBuffer);

	WriteTimestamp(commandBuffer, true);

	vkCmdEndDebugUtilsLabelEXT(commandBuffer);

	result = vkEndCommandBuffer(commandBuffer);
	CheckVulkanResult("Failed to record / end the command buffer", result, nameof(vkEndCommandBuffer));
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

		Window::PollMessages();

		currentTime = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - beginTime).count(); // get the time that elapsed from the beginning till now
	}
}

void Renderer::OnResize()
{
	viewportWidth = testWindow->GetWidth() * internalScale;
	viewportHeight = testWindow->GetHeight() * internalScale;

	swapchain->Recreate(renderPass, false);
	if (canRayTrace)
		rayTracer->RecreateImage(viewportWidth, viewportHeight);

	UpdateScreenShaderTexture(currentFrame);
	
	testWindow->resized = false;
	Console::WriteLine("Resized to " + std::to_string(testWindow->GetWidth()) + 'x' + std::to_string(testWindow->GetHeight()) + " px (" + std::to_string(int(internalScale * 100)) + "%% scale)");
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

inline bool ObjectIsValid(Object* object, bool checkBLAS)
{
	return object->HasFinishedLoading() && object->state == OBJECT_STATE_VISIBLE && (checkBLAS ? object->mesh.IsValid() : true) && object->mesh.HasFinishedLoading() && !object->shouldBeDestroyed;
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
	receivedObjects = 0;
	renderedObjects = 0;

	writer->Write();

	ImGui::Render();
}

void Renderer::SubmitRecording()
{
	SubmitRenderingCommandBuffer(currentFrame, imageIndex);
	PresentSwapchainImage(currentFrame, imageIndex);
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	ResetImGui();
}

inline void GetAllObjectsFromObject(std::vector<Object*>& ret, Object* obj, bool checkBLAS)
{
	if (!ObjectIsValid(obj, checkBLAS))
		return;

	ret.push_back(obj);
	for (Object* object : obj->children)
	{
		GetAllObjectsFromObject(ret, object, checkBLAS);
	}
}

void Renderer::RenderObjects(const std::vector<Object*>& objects, Camera* camera)
{
	std::vector<Object*> activeObjects;
	for (Object* object : objects)
	{
		GetAllObjectsFromObject(activeObjects, object, canRayTrace);
	}

	receivedObjects += objects.size();
	renderedObjects += activeObjects.size();
	
	if (activeObjects.empty())
		return;

	std::lock_guard<std::mutex> lockGuard(drawingMutex);

	//UpdateBindlessTextures(currentFrame, activeObjects);
	WriteIndirectDrawParameters(activeObjects);
	
	RecordCommandBuffer(commandBuffers[currentFrame], imageIndex, activeObjects, camera);
}

void Renderer::StartRenderPass(VkCommandBuffer commandBuffer, VkRenderPass renderPass, glm::vec3 clearColor, VkFramebuffer framebuffer)
{
	if (framebuffer == VK_NULL_HANDLE)
		framebuffer = resultFramebuffer;

	std::array<VkClearValue, 2> clearColors{};
	clearColors[0].color = { clearColor.x, clearColor.y, clearColor.z, 1 };
	clearColors[1].depthStencil = { 1, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = framebuffer;
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = { framebufferWidth, framebufferHeight};
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
	renderPassBeginInfo.pClearValues = clearColors.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void Renderer::EndRenderPass(VkCommandBuffer commandBuffer)
{
	vkCmdEndRenderPass(commandBuffer);
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
		VkDrawIndexedIndirectCommand parameter{};
		parameter.indexCount    = static_cast<uint32_t>(objects[i]->mesh.indices.size());
		parameter.firstIndex    = static_cast<uint32_t>(g_indexBuffer.GetItemOffset(objects[i]->mesh.indexMemory));
		parameter.vertexOffset  = static_cast<uint32_t>(g_vertexBuffer.GetItemOffset(objects[i]->mesh.vertexMemory));
		parameter.instanceCount = 1;

		parameters.push_back(parameter);
	}
	indirectDrawParameters.SubmitNewData(parameters);
}

void Renderer::UpdateScreenShaderTexture(uint32_t currentFrame, VkImageView imageView)
{
	if (resultImage != VK_NULL_HANDLE)
	{
		vkDestroyImage(logicalDevice, resultImage, nullptr);
		vkDestroyImageView(logicalDevice, resultView, nullptr);
		vkFreeMemory(logicalDevice, resultMemory, nullptr);

		vkDestroyImage(logicalDevice, resultDepth, nullptr);
		vkDestroyImageView(logicalDevice, depthView, nullptr);
		vkFreeMemory(logicalDevice, depthMemory, nullptr);

		vkDestroyFramebuffer(logicalDevice, resultFramebuffer, nullptr);
	}

	Vulkan::CreateImage(viewportWidth, viewportHeight, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, resultImage, resultMemory);
	resultView = Vulkan::CreateImageView(resultImage, VK_IMAGE_VIEW_TYPE_2D, 1, 1, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

	VkFormat depthFormat = physicalDevice.GetDepthFormat();
	Vulkan::CreateImage(viewportWidth, viewportHeight, 1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, resultDepth, depthMemory);
	depthView = Vulkan::CreateImageView(resultDepth, VK_IMAGE_VIEW_TYPE_2D, 1, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	std::array<VkImageView, 2> attachments =
	{
		resultView,
		depthView
	};

	VkFramebufferCreateInfo framebufferCreateInfo{};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = renderPass;
	framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferCreateInfo.pAttachments = attachments.data();
	framebufferCreateInfo.width = viewportWidth;
	framebufferCreateInfo.height = viewportHeight;
	framebufferCreateInfo.layers = 1;

	framebufferWidth  = framebufferCreateInfo.width;
	framebufferHeight = framebufferCreateInfo.height;

	VkResult result = vkCreateFramebuffer(logicalDevice, &framebufferCreateInfo, nullptr, &resultFramebuffer);
	CheckVulkanResult("Failed to create the result framebuffer", result, vkCreateFramebuffer);

	imageView = (shouldRasterize || !canRayTrace) ? resultView : rayTracer->gBufferViews[0];
	writer->WriteImage(descriptorSets[currentFrame], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, imageView, resultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	writer->Write(); // do a forced write here since it is critical that this view gets updated as fast as possible, without any buffering from the writer
}

std::vector<Object*> processedObjects;
void Renderer::UpdateBindlessTextures(uint32_t currentFrame, const std::vector<Object*>& objects)
{
	for (int i = 0; i < Mesh::materials.size(); i++)
	{
		if (processedMaterials.count(i) > 0 && processedMaterials[i] == Mesh::materials[i].handle)
			continue;

		for (int j = 0; j < deferredMaterialTextures.size(); j++)
		{
			writer->WriteImage(descriptorSets[currentFrame], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, Mesh::materials[i][deferredMaterialTextures[j]]->imageView, defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); // dont have any test to run this on, so I just hope this works
		}
		processedMaterials[i] = Mesh::materials[i].handle;
	}
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

void Renderer::AddLight(glm::vec3 pos)
{
	for (RenderPipeline* renderPipeline : renderPipelines)
		renderPipeline->AddLight(pos); // this wont add the light to any render pipeline created after this moment
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

void Renderer::SetViewport(VkCommandBuffer commandBuffer, VkExtent2D extent)
{
	VkViewport viewport{};
	Vulkan::PopulateDefaultViewport(viewport, extent);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
}

void Renderer::SetScissors(VkCommandBuffer commandBuffer, VkExtent2D extent)
{
	VkRect2D scissor{};
	Vulkan::PopulateDefaultScissors(scissor, extent);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
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
	queryPool.Fetch();

	animationTime = (queryPool[1] - queryPool[0]) * 0.000001f; // nanoseconds to milliseconds
	rebuildingTime = (queryPool[3] - queryPool[2]) * 0.000001f;
	rayTracingTime = (queryPool[5] - queryPool[4]) * 0.000001f;
	if (denoiseOutput)
	{
		denoisingPrepTime = (queryPool[7] - queryPool[6]) * 0.000001f;
		finalRenderPassTime = (queryPool[9] - queryPool[8]) * 0.000001f;
	}
	else
	{
		finalRenderPassTime = (queryPool[7] - queryPool[6]) * 0.000001f;
		denoisingPrepTime = 0;
		denoisingTime = 0;
	}
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

RenderPipeline::Payload Renderer::GetPipelinePayload(VkCommandBuffer commandBuffer, Camera* camera)
{
	RenderPipeline::Payload ret{};
	ret.renderer = this;
	ret.camera = camera;
	ret.commandBuffer = commandBuffer;
	ret.width = viewportWidth;
	ret.height = viewportHeight;
	ret.window = testWindow;
	
	return ret;
}