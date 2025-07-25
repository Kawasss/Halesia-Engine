#include <vector>
#include <array>
#include <filesystem>
#include <iostream>
#include <vulkan/vk_enum_string_helper.h>

#include "renderer/Vulkan.h"
#include "renderer/Swapchain.h"
#include "renderer/Surface.h"
#include "renderer/Texture.h"
#include "renderer/Intro.h"
#include "renderer/Mesh.h"
#include "renderer/RayTracing.h"
#include "renderer/AnimationManager.h"
#include "renderer/PipelineCreator.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/RenderPipeline.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/GarbageManager.h"
#include "renderer/HdrConverter.h"
#include "renderer/VulkanAPIError.h"
#include "renderer/Light.h"

#include "system/Window.h"
#include "system/System.h"

#include "core/Console.h"
#include "core/Object.h"
#include "core/Camera.h"
#include "core/MeshObject.h"
#include "core/LightObject.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS_IMPLEMENTED
#include <imgui-1.91.7/implot.h>
#include <imgui-1.91.7/ImGuizmo.h>
#include <imgui-1.91.7/imgui.h>
#include <imgui-1.91.7/backends/imgui_impl_vulkan.h>
#include <imgui-1.91.7/backends/imgui_impl_win32.h>

#include "renderer/Renderer.h"

#include "io/IO.h"

namespace fs = std::filesystem;

struct Renderer::LightBuffer
{
	int count;
	LightGPU lights[1024];
};

StorageBuffer<Vertex>   Renderer::g_vertexBuffer;
StorageBuffer<uint32_t> Renderer::g_indexBuffer;
StorageBuffer<Vertex>   Renderer::g_defaultVertexBuffer;

VkSampler Renderer::defaultSampler  = VK_NULL_HANDLE;
VkSampler Renderer::noFilterSampler = VK_NULL_HANDLE;
Handle    Renderer::selectedHandle = 0;
bool      Renderer::shouldRenderCollisionBoxes = false;
bool      Renderer::denoiseOutput = true;
bool      Renderer::canRayTrace = false;
float     Renderer::internalScale = 1;

VkMemoryAllocateFlagsInfo allocateFlagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0 };

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

Renderer::Renderer(Window* window, RendererFlags flags)
{
	this->flags = flags;
	testWindow = window;
	Vulkan::optionalMemoryAllocationFlags = &allocateFlagsInfo;
	InitVulkan();
}

Renderer::~Renderer()
{
	Destroy();
}

void Renderer::Destroy()
{
	::vkDeviceWaitIdle(logicalDevice);

	Vulkan::DeleteSubmittedObjects();
	Vulkan::DestroyAllCommandPools();

	if (!Vulkan::GetContext().IsValid()) // cannot destroy anything if vulkan isnt initialized yet
		return;

	lightBuffer.~Buffer();

	for (Material& mat : Mesh::materials)
		mat.Destroy();

	materials.clear();

	g_defaultVertexBuffer.Destroy();
	g_vertexBuffer.Destroy();
	g_indexBuffer.Destroy();

	queryPool.Destroy();

	for (RenderPipeline* renderPipeline : renderPipelines)
	{
		delete renderPipeline;
	}

	delete animationManager;

	HdrConverter::End();
	//delete rayTracer;

	managedSet.Destroy();

	::ImGui_ImplVulkan_Shutdown();
	::vkDestroyDescriptorPool(logicalDevice, imGUIDescriptorPool, nullptr);

	delete swapchain;

	::vkDestroySampler(logicalDevice, defaultSampler, nullptr);
	::vkDestroySampler(logicalDevice, noFilterSampler, nullptr);

	framebuffer.~Framebuffer();

	Texture::DestroyPlaceholderTextures();

	delete screenPipeline;

	::vkDestroyRenderPass(logicalDevice, renderPass, nullptr);
	::vkDestroyRenderPass(logicalDevice, GUIRenderPass, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		::vkDestroySemaphore(logicalDevice, imageAvaibleSemaphores[i], nullptr);
		::vkDestroySemaphore(logicalDevice, renderFinishedSemaphores[i], nullptr);

		::vkDestroyFence(logicalDevice, inFlightFences[i], nullptr);
	}

	::vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

	Vulkan::Destroy();
	vgm::ForceDelete();

	::vkDestroyDevice(logicalDevice, nullptr);

	surface.Destroy();

	::vkDestroyInstance(instance, nullptr);
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

	VkResult result = ::vkCreateDescriptorPool(logicalDevice, &poolCreateInfo, nullptr, &imGUIDescriptorPool);
	CheckVulkanResult("Failed to create the descriptor pool for imGUI", result);

	ImGui::CreateContext();
	ImPlot::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	::ImGui_ImplWin32_Init(testWindow->GetHandle());

	testWindow->additionalPollCallback = ImGui_ImplWin32_WndProcHandler;

	ImGui_ImplVulkan_InitInfo imGuiCreateInfo{};
	imGuiCreateInfo.Instance = instance;
	imGuiCreateInfo.PhysicalDevice = physicalDevice.Device();
	imGuiCreateInfo.Device = logicalDevice;
	imGuiCreateInfo.Queue = graphicsQueue;
	imGuiCreateInfo.DescriptorPool = imGUIDescriptorPool;
	imGuiCreateInfo.MinImageCount = 3;
	imGuiCreateInfo.ImageCount = 3;
	imGuiCreateInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	imGuiCreateInfo.RenderPass = GUIRenderPass;
	imGuiCreateInfo.CheckVkResultFn = check_vk_result;

	::ImGui_ImplVulkan_Init(&imGuiCreateInfo);

	ResetImGUI();
}

void Renderer::InitVulkan()
{
	Vulkan::Init();

	CreatePhysicalDevice();

	CheckForInterference();

	CreateContext();
	
	CreateDefaultObjects();

	CreateSwapchain();

	CreateImGUI();

	StartRecording();
}

void Renderer::CheckForInterference()
{
	uint32_t numTools = DetectExternalTools();
	if (flags & NoValidation || numTools > 0)
	{
		const char* reason = numTools > 0 ? "Disabled validation layers due to possible interference of external tools\n" : "Disabled validation layers because the flag NO_VALIDATION was set\n";
		Console::WriteLine(reason);
		Vulkan::DisableValidationLayers();
	}
}

void Renderer::InitializeViewport()
{
	viewportWidth  = static_cast<uint32_t>(testWindow->GetWidth()  * internalScale);
	viewportHeight = static_cast<uint32_t>(testWindow->GetHeight() * internalScale);

	UpdateScreenShaderTexture(0);
}

void Renderer::CreateSwapchain()
{
	swapchain = new Swapchain(surface, testWindow, false);
	swapchain->CreateImageViews();
	swapchain->CreateDepthBuffers();

	CreateGUIRenderPass();

	swapchain->CreateFramebuffers(GUIRenderPass);

	GraphicsPipeline::CreateInfo createInfo{}; // the pipeline used to draw to the swapchain
	createInfo.vertexShader   = "shaders/uncompiled/screen.vert";
	createInfo.fragmentShader = "shaders/uncompiled/screen.frag";
	createInfo.renderPass = GUIRenderPass;
	createInfo.noVertices = true;

	screenPipeline = new GraphicsPipeline(createInfo);

	InitializeViewport();
}

void Renderer::CreateGlobalBuffers()
{
	static bool initGlobalBuffers = false;
	if (initGlobalBuffers)
		return;

	const VkBufferUsageFlags rayTracingFlags = canRayTrace ?  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0;

	g_defaultVertexBuffer.Reserve(1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	g_vertexBuffer.Reserve(1024, rayTracingFlags | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	g_indexBuffer.Reserve(1024,  rayTracingFlags | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	initGlobalBuffers = true;
}

void Renderer::CreateDefaultObjects() // default objects are objects that are alive for the entire duration of the renderer and wont really change
{
	CreateCommandPool();
	CreateCommandBuffer();

	Create3DRenderPass();
	CreateGlobalBuffers();
	CreateSyncObjects();

	Texture::GeneratePlaceholderTextures();

	Mesh::AddMaterial({ Texture::placeholderAlbedo, Texture::placeholderNormal, Texture::placeholderMetallic, Texture::placeholderRoughness, Texture::placeholderAmbientOcclusion });
	Mesh::materials.front().OverrideReferenceCount(INT_MAX);

	if (defaultSampler == VK_NULL_HANDLE)
		CreateTextureSampler();

	animationManager = AnimationManager::Get();

	queryPool.Create(VK_QUERY_TYPE_TIMESTAMP, 10);

	lightBuffer.Init(sizeof(LightBuffer), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	lightBuffer.MapPermanently();

	HdrConverter::Start();

	managedSet.Create();
}

void Renderer::CreateContext()
{
	AddExtensions();

	SetLogicalDevice();
}

void Renderer::CreatePhysicalDevice()
{
	instance = Vulkan::GenerateInstance();

	#ifdef _DEBUG
	std::cout << "enabled instance extensions:\n";
	for (const char* extension : Vulkan::GetInstanceExtensions())
		std::cout << "  " << extension << '\n';
	#endif // _DEBUG

	surface = Surface::GenerateSurface(instance, testWindow);
	physicalDevice = Vulkan::GetBestPhysicalDevice(instance, surface);
}

void Renderer::AddExtensions()
{
	canRayTrace = Vulkan::LogicalDeviceExtensionIsSupported(physicalDevice, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) && !(flags & NoRayTracing);
	if (!canRayTrace)
		shouldRasterize = true;
	else
	{
		Vulkan::AddDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		Vulkan::AddDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		Vulkan::AddDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		Vulkan::AddDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
		Vulkan::AddDeviceExtension(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME);
	}
}

uint32_t Renderer::DetectExternalTools()
{
	uint32_t numTools = 0;
	::vkGetPhysicalDeviceToolProperties(physicalDevice.Device(), &numTools, nullptr);
	Console::WriteLine("Detected external tools:");
	if (numTools == 0)
	{
		Console::WriteLine("  None\n");
		return numTools;
	}

	std::vector<VkPhysicalDeviceToolProperties> properties(numTools);
	::vkGetPhysicalDeviceToolProperties(physicalDevice.Device(), &numTools, properties.data());
	for (unsigned int i = 0; i < numTools; i++)
	{
		Console::WriteLine("  name: {}\n  version: {}\n  purposes: {}\n  description: {}", properties[i].name, properties[i].version, ::string_VkToolPurposeFlags(properties[i].purposes), properties[i].description);
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
	createInfo.maxLod = VK_LOD_CLAMP_NONE;

	VkResult result = ::vkCreateSampler(logicalDevice, &createInfo, nullptr, &defaultSampler);
	CheckVulkanResult("Failed to create the texture sampler", result);

	createInfo.magFilter = createInfo.minFilter = VK_FILTER_NEAREST;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

	result = ::vkCreateSampler(logicalDevice, &createInfo, nullptr, &noFilterSampler);
	CheckVulkanResult("Failed to create the texture sampler", result);

	resultSampler = flags & NoFilteringOnResult ? noFilterSampler : defaultSampler;
}

void Renderer::Create3DRenderPass()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	RenderPassBuilder builder3D(VK_FORMAT_R16G16B16A16_UNORM);

	builder3D.SetInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	builder3D.SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	builder3D.ClearOnLoad(true);

	renderPass = builder3D.Build();

	Vulkan::SetDebugName(renderPass, "Default 3D render pass");
}

void Renderer::CreateGUIRenderPass()
{
	RenderPassBuilder builder(swapchain->format);

	builder.SetInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	builder.SetFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	GUIRenderPass = builder.Build();

	Vulkan::SetDebugName(GUIRenderPass, "Default GUI render pass");
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
	allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocationInfo.commandPool = commandPool;
	allocationInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	Vulkan::AllocateCommandBuffers(allocationInfo, commandBuffers);

	#ifdef _DEBUG

	for (CommandBuffer commandBuffer : commandBuffers)
		Vulkan::SetDebugName(commandBuffer.Get(), "drawing command buffer");

	#endif
}

void Renderer::RendererManagedSet::Create()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkPhysicalDeviceProperties props{};
	vkGetPhysicalDeviceProperties(ctx.physicalDevice.Device(), &props);

	uint32_t imageCount = props.limits.maxDescriptorSetSampledImages < MAX_BINDLESS_TEXTURES ? props.limits.maxDescriptorSetSampledImages : MAX_BINDLESS_TEXTURES;

	VkDescriptorPoolSize poolSize{};
	poolSize.descriptorCount = imageCount;
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolCreateInfo.maxSets = 1;
	poolCreateInfo.poolSizeCount = 1;
	poolCreateInfo.pPoolSizes = &poolSize;

	VkResult result = vkCreateDescriptorPool(ctx.logicalDevice, &poolCreateInfo, nullptr, &pool);
	CheckVulkanResult("Failed to create the renderer managed descriptor pool", result);

	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = MATERIAL_BUFFER_BINDING;
	layoutBinding.descriptorCount = MAX_BINDLESS_TEXTURES;
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBinding.stageFlags = VK_SHADER_STAGE_ALL;

	VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo flagCreateInfo{};
	flagCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	flagCreateInfo.bindingCount = 1;
	flagCreateInfo.pBindingFlags = &flags;

	VkDescriptorSetLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutCreateInfo.pNext = &flagCreateInfo;
	layoutCreateInfo.bindingCount = 1;
	layoutCreateInfo.pBindings = &layoutBinding;
	
	result = vkCreateDescriptorSetLayout(ctx.logicalDevice, &layoutCreateInfo, nullptr, &layout);
	CheckVulkanResult("Failed to create renderer managed set layout", result);

	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = pool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = &layout;

	result = vkAllocateDescriptorSets(ctx.logicalDevice, &allocateInfo, &set);
	CheckVulkanResult("Failed to allocate renderer managed descriptor sets", result);

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = VK_NULL_HANDLE;
	imageInfo.sampler = defaultSampler;

	std::vector<VkDescriptorImageInfo> imageInfos(MAX_BINDLESS_TEXTURES, imageInfo);

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.descriptorCount = MAX_BINDLESS_TEXTURES;
	writeSet.dstBinding = MATERIAL_BUFFER_BINDING;
	writeSet.pImageInfo = imageInfos.data();
	writeSet.dstArrayElement = 0;
	writeSet.dstSet = set;
	
	vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, 1, &writeSet, 0, nullptr);

	Pipeline::globalSetLayouts.push_back(layout);
	Pipeline::globalDescriptorSets.push_back(set);

	Vulkan::SetDebugName(layout, "global set layout");
	Vulkan::SetDebugName(set, "global set");
}

void Renderer::RendererManagedSet::Destroy()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	vkDestroyDescriptorSetLayout(ctx.logicalDevice, layout, nullptr);
	vkDestroyDescriptorPool(ctx.logicalDevice, pool, nullptr);
}

void Renderer::UpdateMaterialBuffer()
{
	uint32_t min = static_cast<uint32_t>(std::min(Mesh::materials.size(), materials.size()));
	uint32_t differentIndex = 0; // the index where the mesh::materials and materials start to differ
	bool different = false;

	for (; differentIndex < min; differentIndex++)
	{
		if (Mesh::materials[differentIndex].handle != materials[differentIndex])
		{
			different = true;
			break;
		}
	}

	if (Mesh::materials.size() <= materials.size() && !different)
		return;

	const size_t pbrSize = Material::pbrTextures.size();

	std::vector<VkDescriptorImageInfo> imageInfos((Mesh::materials.size() - differentIndex) * pbrSize);

	materials.resize(differentIndex + 1);
	for (uint32_t i = min; i < materials.size(); i++)
	{
		materials[i] = Mesh::materials[i].HasFinishedLoading() ? Mesh::materials[i].handle : 0;
	}

	for (uint32_t i = differentIndex; i < Mesh::materials.size(); i++)
	{
		for (size_t j = 0; j < pbrSize; j++)
		{
			const Texture* tex = Mesh::materials[i][j];

			size_t index = (i - differentIndex) * pbrSize + j;
			imageInfos[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[index].imageView = tex->imageView;
			imageInfos[index].sampler = defaultSampler;
		}
	}

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.descriptorCount = static_cast<uint32_t>(imageInfos.size());
	writeSet.dstArrayElement = differentIndex * pbrSize;
	writeSet.dstBinding = MATERIAL_BUFFER_BINDING;
	writeSet.dstSet = managedSet.set;
	writeSet.pImageInfo = imageInfos.data();

	vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, 1, &writeSet, 0, nullptr);
}

void Renderer::ProcessRenderPipeline(RenderPipeline* pipeline)
{
	pipeline->renderPass = renderPass;
	pipeline->Start(GetPipelinePayload(GetActiveCommandBuffer(), nullptr));
	renderPipelines.push_back(pipeline);
}

void Renderer::RecordCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex, std::vector<MeshObject*> objects, Camera* camera)
{
	CheckForBufferResizes();

	Vulkan::InsertDebugLabel(commandBuffer.Get(), "drawing buffer");

	queryPool.Reset(commandBuffer);

	//animationManager->ApplyAnimations(commandBuffer); // not good

	camera->SetAspectRatio((float)viewportWidth / (float)viewportHeight);

	RunRenderPipelines(commandBuffer, camera, objects);

	commandBuffer.BeginDebugUtilsLabel("UI");

	SetViewport(commandBuffer.Get(), swapchain->extent);
	SetScissors(commandBuffer.Get(), swapchain->extent);

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

	commandBuffer.BeginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	queryPool.BeginTimestamp(commandBuffer, "final pass");

	glm::vec4 offsets = glm::vec4(viewportOffsets.x, viewportOffsets.y, viewportTransModifiers.x, viewportTransModifiers.y);
	
	screenPipeline->Bind(commandBuffer.Get());
	screenPipeline->PushConstant(commandBuffer, offsets, VK_SHADER_STAGE_VERTEX_BIT);

	commandBuffer.Draw(6, 1, 0, 0);

	RenderImGUI(commandBuffer);
	commandBuffer.EndRenderPass();

	framebuffer.TransitionFromReadToWrite(commandBuffer);

	queryPool.EndTimestamp(commandBuffer, "final pass");

	commandBuffer.EndDebugUtilsLabel();
}

void Renderer::RunRenderPipelines(CommandBuffer commandBuffer, Camera* camera, const std::vector<MeshObject*>& objects)
{
	UpdateMaterialBuffer();

	VkExtent2D viewportExtent = { viewportWidth, viewportHeight };

	RenderPipeline::Payload payload = GetPipelinePayload(commandBuffer, camera);
	for (RenderPipeline* renderPipeline : renderPipelines)
	{
		Vulkan::StartDebugLabel(commandBuffer.Get(), dbgPipelineNames[renderPipeline] + "::Execute");

		queryPool.BeginTimestamp(commandBuffer, dbgPipelineNames[renderPipeline]);

		SetViewport(commandBuffer, viewportExtent);
		SetScissors(commandBuffer, viewportExtent);

		commandBuffer.SetCullMode(VK_CULL_MODE_BACK_BIT);

		renderPipeline->Execute(payload, objects);
		commandBuffer.EndDebugUtilsLabel();

		queryPool.EndTimestamp(commandBuffer, dbgPipelineNames[renderPipeline]);
	}
}

void Renderer::CheckForBufferResizes()
{
	if (!g_vertexBuffer.HasResized() && !g_defaultVertexBuffer.HasResized() && !g_indexBuffer.HasResized())
		return;

	RenderPipeline::Payload payload = GetPipelinePayload(GetActiveCommandBuffer(), nullptr); // maybe move this inside the main render recording to allow pipelines to use that command buffer ??

	for (RenderPipeline* pipeline : renderPipelines)
		pipeline->OnRenderingBufferResize(payload);

	DescriptorWriter::Write(); // force a write, because rendering will immediately start over this check
}

void Renderer::RenderImGUI(CommandBuffer commandBuffer)
{
	ImGui::Render();
	::ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.Get());
}

void Renderer::BindBuffersForRendering(CommandBuffer commandBuffer)
{
	commandBuffer.BindVertexBuffer(g_vertexBuffer.GetBufferHandle(), 0);
	commandBuffer.BindIndexBuffer(g_indexBuffer.GetBufferHandle(), 0, VK_INDEX_TYPE_UINT32);
}

void Renderer::RenderMesh(CommandBuffer commandBuffer, const Mesh& mesh, uint32_t instanceCount)
{
	uint32_t indexCount    = static_cast<uint32_t>(mesh.indices.size());
	uint32_t firstIndex    = static_cast<uint32_t>(g_indexBuffer.GetItemOffset(mesh.indexMemory));
	int32_t  vertexOffset  = static_cast<int32_t>(g_vertexBuffer.GetItemOffset(mesh.vertexMemory));
	uint32_t firstInstance = 0;

	commandBuffer.DrawIndexed(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void Renderer::CreateSyncObjects()
{
	imageAvaibleSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		imageAvaibleSemaphores[i] = Vulkan::CreateSemaphore();
		renderFinishedSemaphores[i] = Vulkan::CreateSemaphore();
		inFlightFences[i] = Vulkan::CreateFence(VK_FENCE_CREATE_SIGNALED_BIT);
	}
}

void Renderer::RenderIntro(Intro* intro)
{
	std::chrono::steady_clock::time_point beginTime = std::chrono::high_resolution_clock::now();
	float currentTime = 0;

	while (currentTime < Intro::maxSeconds)
	{
		::vkWaitForFences(logicalDevice, 1, &inFlightFences[0], true, UINT64_MAX);

		uint32_t imageIndex = GetNextSwapchainImage(0);

		::vkResetFences(logicalDevice, 1, &inFlightFences[0]);

		commandBuffers[0].Reset();
		commandBuffers[0].Begin();

		intro->WriteDataToBuffer(currentTime);
		intro->RecordCommandBuffer(commandBuffers[0], imageIndex);

		commandBuffers[0].End();

		SubmitRenderingCommandBuffer(0, imageIndex);
		PresentSwapchainImage(0, imageIndex);

		Window::PollMessages();

		currentTime = std::chrono::duration<float, std::chrono::seconds::period>(std::chrono::high_resolution_clock::now() - beginTime).count(); // get the time that elapsed from the beginning till now
	}
}

void Renderer::OnResize()
{
	if (!testWindow->CanBeRenderedTo()) // the renderer handles invalid window dimensions by basically ignoring the resize and acting like nothing happened
	{
		Console::WriteLine("Ignored a resize (width or height is 0)");
		return;
	}

	viewportWidth  = static_cast<uint32_t>(testWindow->GetWidth()  * viewportTransModifiers.x * internalScale);
	viewportHeight = static_cast<uint32_t>(testWindow->GetHeight() * viewportTransModifiers.y * internalScale);

	swapchain->Recreate(GUIRenderPass, false);

	RenderPipeline::Payload payload = GetPipelinePayload(GetActiveCommandBuffer(), nullptr);
	for (RenderPipeline* renderPipeline : renderPipelines)
		renderPipeline->Resize(payload);

	UpdateScreenShaderTexture(currentFrame);

	testWindow->resized = false;
	shouldResize = false;

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
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || testWindow->resized || shouldResize)
	{
		OnResize();
	}
	else CheckVulkanResult("Failed to present the swap chain image", result);

	frameCount++;
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
	submitInfo.pCommandBuffers = &commandBuffers[frameIndex].Get();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[frameIndex];

	win32::CriticalLockGuard lockGuard(Vulkan::graphicsQueueSection);
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[frameIndex]);
	CheckVulkanResult("Failed to submit the queue", result);

	submittedCount++;
}

void Renderer::ResetImGUI()
{
	::ImGui_ImplVulkan_NewFrame();
	::ImGui_ImplWin32_NewFrame();
	
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
}

void Renderer::StartRecording()
{
	if (!testWindow->CanBeRenderedTo())
		return;

	CheckForVRAMOverflow();

	VkResult result = vkWaitForFences(logicalDevice, 1, &inFlightFences[currentFrame], true, UINT64_MAX);
	CheckVulkanResult("Failed to wait for fences", result);
	result = vkResetFences(logicalDevice, 1, &inFlightFences[currentFrame]);
	CheckVulkanResult("Failed to reset fences", result);

	Vulkan::DeleteSubmittedObjects();
	GetQueryResults();

	imageIndex = GetNextSwapchainImage(currentFrame);

	activeCmdBuffer = commandBuffers[currentFrame];

	activeCmdBuffer.Reset();
	activeCmdBuffer.Begin();

	submittedCount  = 0;
	receivedObjects = 0;
	renderedObjects = 0;

	DescriptorWriter::Write();
}

void Renderer::SubmitRecording()
{
	if (!testWindow->CanBeRenderedTo())
		return;

	activeCmdBuffer.End();

	SubmitRenderingCommandBuffer(currentFrame, imageIndex);
	PresentSwapchainImage(currentFrame, imageIndex);
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	FIF::frameIndex = currentFrame;
	ResetImGUI();

	vgm::CollectGarbage();
}

static bool ObjectIsValidMesh(Object* pObject, bool checkBLAS)
{
	if (!pObject->IsType(Object::InheritType::Mesh))
		return false;

	MeshObject* obj = dynamic_cast<MeshObject*>(pObject);
	return obj->MeshIsValid() && obj->mesh.HasFinishedLoading() && obj->state == OBJECT_STATE_VISIBLE;
}

static bool ObjectIsValidLight(Object* pObject)
{
	return pObject->IsType(Object::InheritType::Light) && pObject->state == OBJECT_STATE_VISIBLE;
}

static void GetAllObjectsFromObject(std::vector<MeshObject*>& ret, std::vector<LightObject*>& lights, Object* obj, bool checkBLAS)
{
	if (::ObjectIsValidMesh(obj, checkBLAS))
		ret.push_back(dynamic_cast<MeshObject*>(obj));

	if (::ObjectIsValidLight(obj))
		lights.push_back(dynamic_cast<LightObject*>(obj));

	const std::vector<Object*>& children = obj->GetChildren();
	for (Object* object : children)
	{
		GetAllObjectsFromObject(ret, lights, object, checkBLAS);
	}
}

void Renderer::RenderObjects(const std::vector<Object*>& objects, Camera* camera)
{
	if (!testWindow->CanBeRenderedTo())
		return;

	std::vector<MeshObject*> activeObjects;
	std::vector<LightObject*> lightObjects;
	for (Object* object : objects)
	{
		::GetAllObjectsFromObject(activeObjects, lightObjects, object, canRayTrace);
	}

	receivedObjects += static_cast<uint32_t>(objects.size());
	renderedObjects += static_cast<uint32_t>(activeObjects.size());

	win32::CriticalLockGuard lockGuard(drawingSection);

	ResetLightBuffer();
	UpdateLightBuffer(lightObjects);

	RecordCommandBuffer(commandBuffers[currentFrame], imageIndex, activeObjects, camera);
}

void Renderer::UpdateLightBuffer(const std::vector<LightObject*>& lights)
{
	LightBuffer* buffer = lightBuffer.GetMappedPointer<LightBuffer>();
	for (int i = 0; i < lights.size(); i++)
	{
		buffer->lights[i] = lights[i]->ToGPUFormat();
	}
	buffer->count = static_cast<int>(lights.size());
}

void Renderer::StartRenderPass(VkRenderPass renderPass, glm::vec3 clearColor, VkFramebuffer framebuffer)
{
	if (framebuffer == VK_NULL_HANDLE)
		framebuffer = this->framebuffer.Get();

	std::array<VkClearValue, 2> clearColors{};
	clearColors[0].color = { clearColor.x, clearColor.y, clearColor.z, 1 };
	clearColors[1].depthStencil = { 1, 0 };

	::VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = framebuffer;
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = { this->framebuffer.GetWidth(), this->framebuffer.GetHeight() };
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
	renderPassBeginInfo.pClearValues = clearColors.data();

	commandBuffers[currentFrame].BeginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void Renderer::StartRenderPass(const Framebuffer& framebuffer, glm::vec3 clearColor)
{
	std::array<VkClearValue, 2> clearColors{};
	clearColors[0].color = { clearColor.x, clearColor.y, clearColor.z, 1 };
	clearColors[1].depthStencil = { 1, 0 };

	::VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = framebuffer.GetRenderPass();
	renderPassBeginInfo.framebuffer = framebuffer.Get();
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = { framebuffer.GetWidth(), framebuffer.GetHeight() };
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
	renderPassBeginInfo.pClearValues = clearColors.data();

	commandBuffers[currentFrame].BeginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void Renderer::UpdateScreenShaderTexture(uint32_t currentFrame, VkImageView imageView)
{
	if (framebuffer.Get() == VK_NULL_HANDLE)
	{
		framebuffer.SetImageUsage(1, VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		framebuffer.Init(renderPass, 1, viewportWidth, viewportHeight, VK_FORMAT_R16G16B16A16_UNORM); // maybe 32 bit float instead of 8 bit ??
	}
	else
	{
		framebuffer.Resize(viewportWidth, viewportHeight); // recreating the framebuffer will use single time commands, even if it is resized inside a render loop, causing wasted time (fix !!)
	}

	imageView = (shouldRasterize || !canRayTrace || rayTracer == nullptr) ? framebuffer.GetViews()[0] : rayTracer->gBufferViews[0];
	screenPipeline->BindImageToName("image", imageView, resultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	DescriptorWriter::Write(); // do a forced write here since it is critical that this view gets updated as fast as possible, without any buffering from the writer
}

void Renderer::RenderCollisionBoxes(const std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage)
{
	/*for (Object* object : objects)
	{
		glm::mat4 localRotationModel = glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.x), glm::vec3(1, 0, 0)) * glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.y), glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.z), glm::vec3(0, 0, 1));
		glm::mat4 scaleModel = glm::scale(glm::identity<glm::mat4>(), object->rigid.shape.data);
		glm::mat4 translationModel = glm::translate(glm::identity<glm::mat4>(), object->transform.position);
		glm::mat4 trans = translationModel * localRotationModel * scaleModel;
		
		::vkCmdPushConstants(commandBuffer, screenPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &trans);
		::vkCmdDraw(commandBuffer, 36, 1, 0, 0);
	}	*/
}

void Renderer::ResetLightBuffer()
{
	lightBuffer.GetMappedPointer<LightBuffer>()->count = 0;
}

void Renderer::CheckForVRAMOverflow()
{
	static uint64_t max = physicalDevice.VRAM();
	if (Vulkan::allocatedMemory > max)
		throw VulkanAPIError("Critical error: out of VRAM");
}

void Renderer::SetLogicalDevice()
{
	logicalDevice = physicalDevice.GetLogicalDevice(surface);
	QueueFamilyIndices indices = physicalDevice.QueueFamilies(surface);
	queueIndex = indices.graphicsFamily.value();

	::vkGetDeviceQueue(logicalDevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
	::vkGetDeviceQueue(logicalDevice, indices.presentFamily.value(), 0, &presentQueue);
	::vkGetDeviceQueue(logicalDevice, indices.computeFamily.value(), 0, &computeQueue);

	Vulkan::InitializeContext({ instance, logicalDevice, physicalDevice, graphicsQueue, queueIndex, presentQueue, indices.presentFamily.value(), computeQueue, indices.computeFamily.value() });

	#ifdef _DEBUG
	std::cout << "enabled device extensions:\n";
	for (const char* extension : Vulkan::GetDeviceExtensions())
		std::cout << "  " << extension << '\n';
	#endif // _DEBUG
}

void Renderer::SetViewport(CommandBuffer commandBuffer, VkExtent2D extent)
{
	VkViewport viewport{};
	Vulkan::PopulateDefaultViewport(viewport, extent);
	commandBuffer.SetViewport(viewport);
}

void Renderer::SetScissors(CommandBuffer commandBuffer, VkExtent2D extent)
{
	VkRect2D scissor{};
	Vulkan::PopulateDefaultScissors(scissor, extent);
	commandBuffer.SetScissor(scissor);
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
	shouldResize = true;
}

void Renderer::SetInternalResolutionScale(float scale)
{
	internalScale = scale;
	shouldResize = true;
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
	VkResult result = ::vkAcquireNextImageKHR(logicalDevice, swapchain->vkSwapchain, UINT64_MAX, imageAvaibleSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		OnResize();
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		throw VulkanAPIError("Failed to acquire the next swap chain image", result);
	return imageIndex;
}

RenderPipeline::Payload Renderer::GetPipelinePayload(CommandBuffer commandBuffer, Camera* camera)
{
	RenderPipeline::Payload ret{};
	ret.renderer = this;
	ret.camera = camera;
	ret.commandBuffer = commandBuffer.Get() == VK_NULL_HANDLE ? activeCmdBuffer : commandBuffer;
	ret.width = viewportWidth;
	ret.height = viewportHeight;
	ret.window = testWindow;
	
	return ret;
}

RenderPipeline* Renderer::GetRenderPipeline(const std::string_view& name)
{
	for (const auto& [pPipeline, pipelineName] : dbgPipelineNames)
	{
		if (name == pipelineName)
			return pPipeline;
	}
	return nullptr;
}

int Renderer::GetLightCount() const
{
	return lightBuffer.GetMappedPointer<LightBuffer>()->count;
}

const std::string& Renderer::GetRenderPipelineName(RenderPipeline* renderPipeline) const
{
	for (const auto& [pipeline, name] : dbgPipelineNames)
		if (renderPipeline == pipeline)
			return name;
	__debugbreak();
	return "";
}

glm::vec2 Renderer::GetViewportOffset() const
{
	return viewportOffsets;
}

glm::vec2 Renderer::GetViewportModifier() const
{
	return viewportTransModifiers;
}

VkRenderPass Renderer::GetDefault3DRenderPass() const
{
	return renderPass;
}

VkRenderPass Renderer::GetNonClearingRenderPass() const
{
	return GUIRenderPass;
}

uint32_t Renderer::GetInternalWidth() const
{
	return viewportWidth;
}

uint32_t Renderer::GetInternalHeight() const 
{ 
	return viewportHeight; 
}

std::map<std::string, uint64_t> Renderer::GetTimestamps() const
{
	return queryPool.GetTimestamps(); 
}

Framebuffer& Renderer::GetFramebuffer()
{
	return framebuffer;
}

CommandBuffer Renderer::GetActiveCommandBuffer() const
{
	return activeCmdBuffer;
}

const FIF::Buffer& Renderer::GetLightBuffer() const
{
	return lightBuffer;
}

const std::vector<RenderPipeline*>& Renderer::GetAllRenderPipelines() const
{
	return renderPipelines; 
}

bool Renderer::CompletedFIFCyle()
{
	return FIF::frameIndex == 0;
}