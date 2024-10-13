#include <vector>
#include <array>
#include <filesystem>
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
#include "renderer/Light.h"

#include "system/Window.h"

#include "core/Console.h"
#include "core/Object.h"
#include "core/Camera.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "implot.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_win32.h"

#include "renderer/Renderer.h"

#include "io/IO.h"

StorageBuffer<Vertex>   Renderer::g_vertexBuffer;
StorageBuffer<uint16_t> Renderer::g_indexBuffer;
StorageBuffer<Vertex>   Renderer::g_defaultVertexBuffer;

VkSampler Renderer::defaultSampler  = VK_NULL_HANDLE;
VkSampler Renderer::noFilterSampler = VK_NULL_HANDLE;
Handle    Renderer::selectedHandle = 0;
bool      Renderer::shouldRenderCollisionBoxes = false;
bool      Renderer::denoiseOutput = true;
bool      Renderer::canRayTrace = false;
float     Renderer::internalScale = 1;

VkMemoryAllocateFlagsInfo allocateFlagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0 };

Renderer::Renderer(Window* window, RendererFlags flags)
{
	this->flags = flags;
	testWindow = window;
	window->additionalPollCallback = ::ImGui_ImplWin32_WndProcHandler;
	Vulkan::optionalMemoryAllocationFlags = &allocateFlagsInfo;
	InitVulkan();
}

void Renderer::Destroy()
{
	::vkDeviceWaitIdle(logicalDevice);

	Vulkan::DeleteSubmittedObjects();
	Vulkan::DestroyAllCommandPools();

	if (!Vulkan::GetContext().IsValid()) // cannot destroy anything if vulkan isnt initialized yet
		return;

	Mesh::materials.front().Destroy(); // the only material whose lifetime is managed by the renderer is the default material

	g_defaultVertexBuffer.Destroy();
	g_vertexBuffer.Destroy();
	g_indexBuffer.Destroy();

	queryPool.Destroy();

	for (RenderPipeline* renderPipeline : renderPipelines)
	{
		renderPipeline->Destroy();
		delete renderPipeline;
	}

	delete writer;

	delete animationManager;
	delete rayTracer;

	::vkDestroyDescriptorPool(logicalDevice, imGUIDescriptorPool, nullptr);
	::ImGui_ImplVulkan_Shutdown();

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
	CheckVulkanResult("Failed to create the descriptor pool for imGUI", result, vkCreateDescriptorPool);

	ImGui::CreateContext();
	ImPlot::CreateContext();

	::ImGui_ImplWin32_Init(testWindow->window);

	ImGui_ImplVulkan_InitInfo imGUICreateInfo{};
	imGUICreateInfo.Instance = instance;
	imGUICreateInfo.PhysicalDevice = physicalDevice.Device();
	imGUICreateInfo.Device = logicalDevice;
	imGUICreateInfo.Queue = graphicsQueue;
	imGUICreateInfo.DescriptorPool = imGUIDescriptorPool;
	imGUICreateInfo.MinImageCount = 3;
	imGUICreateInfo.ImageCount = 3;
	imGUICreateInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	::ImGui_ImplVulkan_Init(&imGUICreateInfo, GUIRenderPass);

	VkCommandBuffer imGUICommandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);
	::ImGui_ImplVulkan_CreateFontsTexture(imGUICommandBuffer);
	Vulkan::EndSingleTimeCommands(graphicsQueue, imGUICommandBuffer, commandPool);

	::ImGui_ImplVulkan_DestroyFontUploadObjects();

	ResetImGUI();
}

void Renderer::RecompileShaders()
{
	if (flags & NO_SHADER_RECOMPILATION)
	{
		Console::WriteLine("Cannot recompile shaders because the flag NO_SHADER_RECOMPILATION was set");
		return;
	}

	Console::WriteLine("Debug mode detected, recompiling all shaders found in directory \"shaders\"...", Console::Severity::Normal);
	auto oldPath = std::filesystem::current_path();
	auto newPath = std::filesystem::absolute("shaders");
	for (const auto& file : std::filesystem::directory_iterator(newPath))
	{
		if (file.path().extension() != ".bat")
			continue;
		std::string shaderComp = file.path().string();
		std::filesystem::current_path(newPath);
		::system(shaderComp.c_str()); // windows only!!
	}
	std::filesystem::current_path(oldPath);
}

void Renderer::InitVulkan()
{
	#ifdef _DEBUG // recompiles all the shaders with their .bat files, this simply makes it less of a hassle to change the shaders
	RecompileShaders();
	#endif

	CreatePhysicalDevice();

	CheckForInterference();

	CreateContext();
	
	CreateDefaultObjects();

	CreateRayTracerCond();

	CreateSwapchain();

	CreateImGUI();
}

void Renderer::CheckForInterference()
{
	uint32_t numTools = DetectExternalTools();
	if (flags & NO_VALIDATION || numTools > 0)
	{
		const char* reason = numTools > 0 ? "Disabled validation layers due to possible interference of external tools\n" : "Disabled validation layers because the flag NO_VALIDATION was set\n";
		Console::WriteLine(reason);
		Vulkan::DisableValidationLayers();
	}
}

void Renderer::InitializeViewport()
{
	viewportWidth  = testWindow->GetWidth()  * internalScale;
	viewportHeight = testWindow->GetHeight() * internalScale;

	UpdateScreenShaderTexture(0);
}

void Renderer::CreateRayTracerCond() // only create if the GPU supports it
{
	shouldRasterize = !canRayTrace;
	if (shouldRasterize)
		return;

	rayTracer = new RayTracingPipeline;
	rayTracer->renderPass = renderPass;
	rayTracer->Start(GetPipelinePayload(VK_NULL_HANDLE, nullptr));
}

void Renderer::CreateSwapchain()
{
	swapchain = new Swapchain(surface, testWindow, false);
	swapchain->CreateImageViews();
	swapchain->CreateDepthBuffers();

	CreateGUIRenderPass();

	swapchain->CreateFramebuffers(GUIRenderPass);

	GraphicsPipeline::CreateInfo createInfo{}; // the pipeline used to draw to the swapchain
	createInfo.vertexShader   = "shaders/spirv/screen.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/screen.frag.spv";
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

	constexpr VkBufferUsageFlags rayTracingFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

	g_defaultVertexBuffer.Reserve(1024, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	g_vertexBuffer.Reserve(1024, (canRayTrace ? rayTracingFlags : 0) | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	g_indexBuffer.Reserve(1024,  (canRayTrace ? rayTracingFlags : 0) | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

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

	writer = DescriptorWriter::Get();
	animationManager = AnimationManager::Get();

	queryPool.Create(VK_QUERY_TYPE_TIMESTAMP, 10);
}

void Renderer::CreateContext()
{
	AddExtensions();

	SetLogicalDevice();
}

void Renderer::CreatePhysicalDevice()
{
	instance = Vulkan::GenerateInstance();

	surface = Surface::GenerateSurface(instance, testWindow);
	physicalDevice = Vulkan::GetBestPhysicalDevice(instance, surface);
}

void Renderer::AddExtensions()
{
	canRayTrace = Vulkan::LogicalDeviceExtensionIsSupported(physicalDevice, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME) && !(flags & NO_RAY_TRACING) && MAX_FRAMES_IN_FLIGHT == 1; // the ray tracer can only handle 1 frame in flight
	if (!canRayTrace)
		shouldRasterize = true;
	else
	{
		Vulkan::AddDeviceExtenion(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
		Vulkan::AddDeviceExtenion(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		Vulkan::AddDeviceExtenion(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		Vulkan::AddDeviceExtenion(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
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
	for (int i = 0; i < numTools; i++)
	{
		Console::WriteLine(
			"\n  name: " + (std::string)properties[i].name +
			"\n  version: " + properties[i].version +
			"\n  purposes: " + ::string_VkToolPurposeFlags(properties[i].purposes) +
			"\n  description: " + properties[i].description);
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
	CheckVulkanResult("Failed to create the texture sampler", result, vkCreateSampler);

	createInfo.magFilter = createInfo.minFilter = VK_FILTER_NEAREST;
	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

	result = ::vkCreateSampler(logicalDevice, &createInfo, nullptr, &noFilterSampler);
	CheckVulkanResult("Failed to create the texture sampler", result, vkCreateSampler);

	resultSampler = flags & NO_FILTERING_ON_RESULT ? noFilterSampler : defaultSampler;
}

void Renderer::Create3DRenderPass()
{
	RenderPassBuilder builder3D(VK_FORMAT_R8G8B8A8_UNORM);

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

void Renderer::ProcessRenderPipeline(RenderPipeline* pipeline)
{
	pipeline->renderPass = renderPass;
	pipeline->Start(GetPipelinePayload(VK_NULL_HANDLE, nullptr));
	renderPipelines.push_back(pipeline);
}

void Renderer::RecordCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex, std::vector<Object*> objects, Camera* camera)
{
	CheckForBufferResizes();

	commandBuffer.Begin();

	Vulkan::InsertDebugLabel(commandBuffer.Get(), "drawing buffer");

	queryPool.Reset(commandBuffer);

	//animationManager->ApplyAnimations(commandBuffer); // not good
	
	//for (Object* obj : objects)
	//	obj->mesh.BLAS->RebuildGeometry(commandBuffer, obj->mesh);

	camera->SetAspectRatio((float)viewportWidth / (float)viewportHeight);

	if (canRayTrace && !shouldRasterize)
	{
		RunRayTracer(commandBuffer, camera, objects);
	}
	else
	{
		RunRenderPipelines(commandBuffer, camera, objects);
	}

	Vulkan::StartDebugLabel(commandBuffer.Get(), "UI");

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

	commandBuffer.EndDebugUtilsLabelEXT();

	commandBuffer.End();
}

void Renderer::RunRayTracer(CommandBuffer commandBuffer, Camera* camera, const std::vector<Object*>& objects)
{
	Vulkan::StartDebugLabel(commandBuffer.Get(), "ray tracing");

	queryPool.BeginTimestamp(commandBuffer, "ray-tracing");

	rayTracer->Execute(GetPipelinePayload(commandBuffer, camera), objects);
	commandBuffer.EndDebugUtilsLabelEXT();

	queryPool.EndTimestamp(commandBuffer, "ray-tracing");	
}

void Renderer::RunRenderPipelines(CommandBuffer commandBuffer, Camera* camera, const std::vector<Object*>& objects)
{
	VkExtent2D viewportExtent = { viewportWidth, viewportHeight };

	RenderPipeline::Payload payload = GetPipelinePayload(commandBuffer, camera);
	for (RenderPipeline* renderPipeline : renderPipelines)
	{
		Vulkan::StartDebugLabel(commandBuffer.Get(), dbgPipelineNames[renderPipeline] + "::Execute");

		queryPool.BeginTimestamp(commandBuffer, dbgPipelineNames[renderPipeline]);

		SetViewport(commandBuffer, viewportExtent);
		SetScissors(commandBuffer, viewportExtent);

		renderPipeline->Execute(payload, objects);
		commandBuffer.EndDebugUtilsLabelEXT();

		queryPool.EndTimestamp(commandBuffer, dbgPipelineNames[renderPipeline]);
	}
}

void Renderer::CheckForBufferResizes()
{
	if (!g_vertexBuffer.HasResized() && !g_defaultVertexBuffer.HasResized() && !g_indexBuffer.HasResized())
		return;

	RenderPipeline::Payload payload = GetPipelinePayload(VK_NULL_HANDLE, nullptr); // maybe move this inside the main render recording to allow pipelines to use that command buffer ??

	for (RenderPipeline* pipeline : renderPipelines)
		pipeline->OnRenderingBufferResize(payload);

	if (!shouldRasterize)
		rayTracer->OnRenderingBufferResize(payload); // ray tracing is still handled seperately from the other pipelines !!

	writer->Write(); // force a write, because rendering will immediately start over this check
}

void Renderer::RenderImGUI(CommandBuffer commandBuffer)
{
	ImGui::Render();
	::ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.Get());
}

void Renderer::BindBuffersForRendering(CommandBuffer commandBuffer)
{
	commandBuffer.BindVertexBuffer(g_vertexBuffer.GetBufferHandle(), 0);
	commandBuffer.BindIndexBuffer(g_indexBuffer.GetBufferHandle(), 0, VK_INDEX_TYPE_UINT16);
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

	viewportWidth  = testWindow->GetWidth()  * viewportTransModifiers.x;
	viewportHeight = testWindow->GetHeight() * viewportTransModifiers.y;

	swapchain->Recreate(GUIRenderPass, false);
	if (canRayTrace)
		rayTracer->RecreateImage(viewportWidth, viewportHeight);

	RenderPipeline::Payload payload = GetPipelinePayload(VK_NULL_HANDLE, nullptr);
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
	submitInfo.pCommandBuffers = &commandBuffers[frameIndex].Get();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[frameIndex];

	win32::CriticalLockGuard lockGuard(Vulkan::graphicsQueueSection);
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[frameIndex]);
	CheckVulkanResult("Failed to submit the queue", result, nameof(vkQueueSubmit));

	submittedCount++;
}

inline bool ObjectIsValid(Object* object, bool checkBLAS)
{
	return object->HasFinishedLoading() && object->state == OBJECT_STATE_VISIBLE && (checkBLAS ? object->mesh.IsValid() : true) && object->mesh.HasFinishedLoading() && !object->ShouldBeDestroyed();
}

void Renderer::ResetImGUI()
{
	::ImGui_ImplVulkan_NewFrame();
	::ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void Renderer::StartRecording()
{
	if (!testWindow->CanBeRenderedTo())
		return;

	CheckForVRAMOverflow();

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

	commandBuffers[currentFrame].Reset();

	submittedCount = 0;
	receivedObjects = 0;
	renderedObjects = 0;

	writer->Write();
}

void Renderer::SubmitRecording()
{
	if (!testWindow->CanBeRenderedTo())
		return;

	SubmitRenderingCommandBuffer(currentFrame, imageIndex);
	PresentSwapchainImage(currentFrame, imageIndex);
	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	FIF::frameIndex = currentFrame;
	ResetImGUI();
}

inline void GetAllObjectsFromObject(std::vector<Object*>& ret, Object* obj, bool checkBLAS)
{
	if (!::ObjectIsValid(obj, checkBLAS))
		return;

	ret.push_back(obj);

	const std::vector<Object*>& children = obj->GetChildren();
	for (Object* object : children)
	{
		GetAllObjectsFromObject(ret, object, checkBLAS);
	}
}

void Renderer::RenderObjects(const std::vector<Object*>& objects, Camera* camera)
{
	if (!testWindow->CanBeRenderedTo())
		return;

	std::vector<Object*> activeObjects;
	for (Object* object : objects)
	{
		::GetAllObjectsFromObject(activeObjects, object, canRayTrace);
	}

	receivedObjects += objects.size();
	renderedObjects += activeObjects.size();

	win32::CriticalLockGuard lockGuard(drawingSection);

	RecordCommandBuffer(commandBuffers[currentFrame], imageIndex, activeObjects, camera);
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
		framebuffer.Init(renderPass, 1, viewportWidth, viewportHeight, VK_FORMAT_R8G8B8A8_UNORM); // maybe 32 bit float instead of 8 bit ??
	}
	else
	{
		framebuffer.Resize(viewportWidth, viewportHeight); // recreating the framebuffer will use single time commands, even if it is resized inside a render loop, causing wasted time (fix !!)
	}

	imageView = (shouldRasterize || !canRayTrace || rayTracer == nullptr) ? framebuffer.GetViews()[0] : rayTracer->gBufferViews[0];
	screenPipeline->BindImageToName("image", imageView, resultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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
			writer->WriteImage(screenPipeline->GetDescriptorSets()[currentFrame], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, Mesh::materials[i][deferredMaterialTextures[j]]->imageView, defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL); // dont have any test to run this on, so I just hope this works
		}
		processedMaterials[i] = Mesh::materials[i].handle;
	}
}

void Renderer::RenderCollisionBoxes(const std::vector<Object*>& objects, VkCommandBuffer commandBuffer, uint32_t currentImage)
{
	for (Object* object : objects)
	{
		if (object->rigid.shape.type != Shape::Type::Box)
			continue;

		glm::mat4 localRotationModel = glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.x), glm::vec3(1, 0, 0)) * glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.y), glm::vec3(0, 1, 0)) * glm::rotate(glm::mat4(1), glm::radians(object->transform.rotation.z), glm::vec3(0, 0, 1));
		glm::mat4 scaleModel = glm::scale(glm::identity<glm::mat4>(), object->rigid.shape.data);
		glm::mat4 translationModel = glm::translate(glm::identity<glm::mat4>(), object->transform.position);
		glm::mat4 trans = translationModel * localRotationModel * scaleModel;
		
		::vkCmdPushConstants(commandBuffer, screenPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &trans);
		::vkCmdDraw(commandBuffer, 36, 1, 0, 0);
	}	
}

void Renderer::AddLight(const Light& light)
{
	for (RenderPipeline* renderPipeline : renderPipelines)
		renderPipeline->AddLight(light); // this wont add the light to any render pipeline created after this moment
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
		throw VulkanAPIError("Failed to acquire the next swap chain image", result, nameof(vkAcquireNextImageKHR), __FILENAME__, __LINE__);
	return imageIndex;
}

RenderPipeline::Payload Renderer::GetPipelinePayload(CommandBuffer commandBuffer, Camera* camera)
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