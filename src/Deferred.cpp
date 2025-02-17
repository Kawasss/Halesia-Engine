#include "renderer/Deferred.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Renderer.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/accelerationStructures.h"
#include "renderer/PipelineCreator.h"
#include "renderer/GarbageManager.h"
#include "renderer/Skybox.h"

#include "core/Object.h"
#include "core/Camera.h"

struct PushConstant
{
	glm::mat4 model;
	int materialID;
};

#include "renderer/RayTracingPipeline.h"

constexpr VkFormat GBUFFER_POSITION_FORMAT = VK_FORMAT_R32G32B32A32_SFLOAT; // 32 bit format takes a lot of performance compared to an 8 bit format
constexpr VkFormat GBUFFER_ALBEDO_FORMAT   = VK_FORMAT_R8G8B8A8_UNORM;
constexpr VkFormat GBUFFER_NORMAL_FORMAT   = VK_FORMAT_R16G16B16A16_SFLOAT; // 16 bit instead of 8 bit to allow for negative normals
constexpr VkFormat GBUFFER_MRAO_FORMAT     = VK_FORMAT_R8G8B8A8_UNORM;      // Metallic, Roughness and Ambient Occlusion
constexpr VkFormat GBUFFER_RTGI_FORMAT     = VK_FORMAT_R8G8B8A8_UNORM;

static constexpr uint32_t RTGI_RESOLUTION_UPSCALE = 2;

void DeferredPipeline::Start(const Payload& payload)
{
	std::array<VkFormat, GBUFFER_COUNT> formats = 
	{ 
		GBUFFER_POSITION_FORMAT,
		GBUFFER_ALBEDO_FORMAT, 
		GBUFFER_NORMAL_FORMAT, 
		GBUFFER_MRAO_FORMAT,
	};

	CreateRenderPass(formats);
	CreatePipelines(renderPass, payload.renderer->GetDefault3DRenderPass());

	framebuffer.Init(renderPass, payload.width, payload.height, formats);

	CreateBuffers();

	BindResources();
	
	if (Renderer::canRayTrace)
	{
		TLAS.reset(TopLevelAccelerationStructure::Create({}));

		CreateAndBindRTGI(payload.width, payload.height);

		BindTLAS();
	}

	SetTextureBuffer();
}

void DeferredPipeline::CreateAndBindRTGI(uint32_t width, uint32_t height)
{
	rtgiPipeline = std::make_unique<RayTracingPipeline>("shaders/spirv/rtgi.rgen.spv", "shaders/spirv/rtgi.rchit.spv", "shaders/spirv/rtgi.rmiss.spv");

	ResizeRTGI(width, height);
}

void DeferredPipeline::PushRTGIConstants(const Payload& payload)
{
	RTGIConstants constants{};
	constants.position = glm::vec4(payload.camera->position, 1.0f);
	constants.viewInv  = glm::inverse(payload.camera->GetViewMatrix());
	constants.projInv  = glm::inverse(payload.camera->GetProjectionMatrix());

	rtgiPipeline->PushConstant(payload.commandBuffer, constants, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
}

void DeferredPipeline::ResizeRTGI(uint32_t width, uint32_t height)
{
	width  /= RTGI_RESOLUTION_UPSCALE;
	height /= RTGI_RESOLUTION_UPSCALE;

	if (rtgiImage.IsValid())
		rtgiImage.Destroy();

	if (rtgiView != VK_NULL_HANDLE)
		vgm::Delete(rtgiView);

	rtgiImage = Vulkan::CreateImage(width, height, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
	rtgiView = Vulkan::CreateImageView(rtgiImage.Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, GBUFFER_RTGI_FORMAT, VK_IMAGE_ASPECT_COLOR_BIT);

	SetRTGIImageLayout();
	BindRTGIResources();
}

void DeferredPipeline::SetRTGIImageLayout()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = rtgiImage.Get();
	memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memoryBarrier.subresourceRange.baseMipLevel = 0;
	memoryBarrier.subresourceRange.levelCount = 1;
	memoryBarrier.subresourceRange.baseArrayLayer = 0;
	memoryBarrier.subresourceRange.layerCount = 1;

	constexpr VkPipelineStageFlags src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	constexpr VkPipelineStageFlags dst = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex); // or grab the command buffer from the renderer
	CommandBuffer cmdBuffer = Vulkan::BeginSingleTimeCommands(commandPool);

	cmdBuffer.PipelineBarrier(src, dst, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

	Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, cmdBuffer.Get(), commandPool);
	Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);
}

void DeferredPipeline::BindRTGIResources()
{
	rtgiPipeline->BindImageToName("globalIlluminationImage", rtgiView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_GENERAL);
	rtgiPipeline->BindImageToName("albedoImage", GetAlbedoView(), Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	rtgiPipeline->BindImageToName("normalImage", GetNormalView(), Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	rtgiPipeline->BindImageToName("positionImage", GetPositionView(), Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void DeferredPipeline::CreateBuffers()
{
	uboBuffer.Init(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	uboBuffer.MapPermanently();

	lightBuffer.Init(sizeof(LightBuffer), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	lightBuffer.MapPermanently();
}

void DeferredPipeline::BindTLAS()
{
	VkWriteDescriptorSetAccelerationStructureKHR ASDescriptorInfo{};
	ASDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
	ASDescriptorInfo.accelerationStructureCount = 1;
	ASDescriptorInfo.pAccelerationStructures = &TLAS->accelerationStructure;

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	writeSet.pNext = &ASDescriptorInfo;
	writeSet.descriptorCount = 1;
	writeSet.dstBinding = 4;

	for (const std::vector<VkDescriptorSet>& sets : secondPipeline->GetAllDescriptorSets())
	{
		writeSet.dstSet = sets[0];
		vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, 1, &writeSet, 0, nullptr); // better to incorporate this into Pipeline
	}

	// also write the TLAS to the RTGI pipeline
	writeSet.dstBinding = 0;
	for (const std::vector<VkDescriptorSet>& sets : rtgiPipeline->GetAllDescriptorSets())
	{
		writeSet.dstSet = sets[0];
		vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, 1, &writeSet, 0, nullptr); // better to incorporate this into Pipeline
	}
}

void DeferredPipeline::BindResources()
{
	firstPipeline->BindBufferToName("ubo", uboBuffer.Get());
	secondPipeline->BindBufferToName("lights", lightBuffer.Get());

	BindGBuffers();
}

void DeferredPipeline::BindGBuffers()
{
	constexpr VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	const VkSampler& sampler = Renderer::defaultSampler;
	const std::vector<VkImageView>& views = framebuffer.GetViews();

	secondPipeline->BindImageToName("positionImage", GetPositionView(), sampler, layout);
	secondPipeline->BindImageToName("albedoImage", GetAlbedoView(), sampler, layout);
	secondPipeline->BindImageToName("normalImage", GetNormalView(), sampler, layout);
	secondPipeline->BindImageToName("metallicRoughnessAOImage", GetMRAOView(), sampler, layout);
}

void DeferredPipeline::CreateRenderPass(const std::array<VkFormat, GBUFFER_COUNT>& formats)
{
	RenderPassBuilder builder(formats);

	builder.SetInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	builder.SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	builder.ClearOnLoad(true);

	renderPass = builder.Build();
}

void DeferredPipeline::CreatePipelines(VkRenderPass firstPass, VkRenderPass secondPass)
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/spirv/deferredFirst.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/deferredFirst.frag.spv";
	createInfo.renderPass = firstPass;
	
	firstPipeline = std::make_unique<GraphicsPipeline>(createInfo);

	createInfo.vertexShader   = "shaders/spirv/deferredSecond.vert.spv";
	createInfo.fragmentShader = Renderer::canRayTrace ? "shaders/spirv/deferredSecondRT.frag.spv" : "shaders/spirv/deferredSecond.frag.spv";
	createInfo.renderPass = secondPass;
	createInfo.noVertices = true;

	secondPipeline = std::make_unique<GraphicsPipeline>(createInfo);
}

Skybox* DeferredPipeline::CreateNewSkybox(const std::string& path)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkCommandPool pool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
	VkCommandBuffer cmdBuffer = Vulkan::BeginSingleTimeCommands(pool);

	Skybox* ret = Skybox::ReadFromHDR(path, cmdBuffer);

	Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, cmdBuffer, pool);
	Vulkan::YieldCommandPool(ctx.graphicsIndex, pool);

	return ret;
}

void DeferredPipeline::LoadSkybox(const std::string& path)
{
	skybox.reset(CreateNewSkybox(path));

	uint32_t width = framebuffer.GetWidth(), height = framebuffer.GetHeight();
	if (width != 0 && height != 0)
		skybox->Resize(width, height);

	skybox->targetView = GetAlbedoView();
	skybox->depth = framebuffer.GetDepthView();

	rtgiPipeline->BindImageToName("skybox", skybox->GetCubemap()->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void DeferredPipeline::Execute(const Payload& payload, const std::vector<Object*>& objects)
{
	UpdateTextureBuffer(); // temp !!!

	if (Renderer::canRayTrace)
	{
		if (!TLAS->HasBeenBuilt() && !objects.empty())
			TLAS->Build(objects);
		TLAS->Update(objects, payload.commandBuffer.Get());
	}

	UpdateUBO(payload.camera);

	const CommandBuffer cmdBuffer = payload.commandBuffer;
	Renderer* renderer = payload.renderer;

	framebuffer.StartRenderPass(cmdBuffer);

	firstPipeline->Bind(cmdBuffer);

	VkBuffer vertexBuffer = Renderer::g_vertexBuffer.GetBufferHandle();
	VkDeviceSize offset = 0;

	Renderer::BindBuffersForRendering(cmdBuffer);

	PushConstant pushConstant{};
	for (Object* obj : objects)
	{
		pushConstant.model = obj->transform.GetModelMatrix();
		pushConstant.materialID = obj->mesh.GetMaterialIndex();

		firstPipeline->PushConstant(cmdBuffer, pushConstant, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

		Renderer::RenderMesh(cmdBuffer, obj->mesh);
	}

	cmdBuffer.EndRenderPass();

	if (skybox != nullptr)
		skybox->Draw(payload.commandBuffer, payload.camera);

	if (Renderer::canRayTrace)
	{
		Vulkan::StartDebugLabel(payload.commandBuffer.Get(), "RTGI");

		PushRTGIConstants(payload);

		payload.commandBuffer.SetCheckpoint(static_cast<const void*>("start of RTGI"));

		rtgiPipeline->Bind(payload.commandBuffer);
		rtgiPipeline->Execute(payload.commandBuffer, payload.width / RTGI_RESOLUTION_UPSCALE, payload.height / RTGI_RESOLUTION_UPSCALE, 1);

		payload.commandBuffer.SetCheckpoint(static_cast<const void*>("end of RTGI"));

		payload.commandBuffer.EndDebugUtilsLabelEXT();
	}

	payload.commandBuffer.SetCheckpoint(static_cast<const void*>("start of final deferred pass"));

	renderer->StartRenderPass(renderer->GetDefault3DRenderPass());

	secondPipeline->Bind(cmdBuffer);

	glm::vec3 camPos = payload.camera->position;

	secondPipeline->PushConstant(cmdBuffer, camPos, VK_SHADER_STAGE_FRAGMENT_BIT);

	cmdBuffer.Draw(6, 1, 0, 0);

	cmdBuffer.EndRenderPass();

	payload.commandBuffer.SetCheckpoint(static_cast<const void*>("end of final deferred pass"));

	framebuffer.TransitionFromReadToWrite(cmdBuffer);
}

void DeferredPipeline::Resize(const Payload& payload)
{
	framebuffer.Resize(payload.width, payload.height);

	BindGBuffers();

	skybox->Resize(payload.width, payload.height);
	skybox->targetView = GetAlbedoView();
	skybox->depth = framebuffer.GetDepthView();

	ResizeRTGI(payload.width, payload.height);
}

void DeferredPipeline::UpdateTextureBuffer()
{
	constexpr size_t MAX_PROCESSED_COUNT = 5;

	const size_t pbrSize = PBRMaterialTextures.size();
	const size_t maxSize = pbrSize * MAX_PROCESSED_COUNT;

	std::vector<uint64_t>& processedMaterials = processedMats[FIF::frameIndex];

	std::vector<VkDescriptorImageInfo> imageInfos(maxSize);
	std::vector<VkWriteDescriptorSet>  writeSets(maxSize);

	int processedCount = 0;

	if (processedMaterials.size() < Mesh::materials.size())
		processedMaterials.resize(Mesh::materials.size());

	for (int i = 0; i < Mesh::materials.size(); i++)
	{
		if (processedCount >= MAX_PROCESSED_COUNT)
			break;

		if (processedMaterials[i] == Mesh::materials[i].handle)
			continue;

		for (int j = 0; j < pbrSize; j++)
		{
			size_t index = i * pbrSize + j;

			VkDescriptorImageInfo& imageInfo = imageInfos[index];
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = Mesh::materials[i][PBRMaterialTextures[j]]->imageView;
			imageInfo.sampler = Renderer::defaultSampler;

			VkWriteDescriptorSet& writeSet = writeSets[index];
			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeSet.descriptorCount = 1;
			writeSet.dstArrayElement = index;
			writeSet.dstBinding = 2;
			writeSet.dstSet = firstPipeline->GetDescriptorSets()[0];
			writeSet.pImageInfo = &imageInfos[index];
		}
		processedMaterials.push_back(Mesh::materials[i].handle);
		processedCount++;
	}

	if (processedCount > 0)
		vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, static_cast<uint32_t>(processedCount * pbrSize), writeSets.data(), 0, nullptr);
}

void DeferredPipeline::SetTextureBuffer()
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = VK_NULL_HANDLE;
	imageInfo.sampler = Renderer::defaultSampler;

	std::vector<VkDescriptorImageInfo> imageInfos(500, imageInfo);

	std::array<VkWriteDescriptorSet, FIF::FRAME_COUNT> writeSets{};

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
	{
		VkWriteDescriptorSet& writeSet = writeSets[i];
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeSet.dstSet = firstPipeline->GetAllDescriptorSets()[i][0];
		writeSet.pImageInfo = imageInfos.data();
		writeSet.descriptorCount = 500;
		writeSet.dstArrayElement = 0;
		writeSet.dstBinding = 2;
	}

	vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, FIF::FRAME_COUNT, writeSets.data(), 0, nullptr);
}

void DeferredPipeline::UpdateUBO(Camera* cam)
{
	UBO* ubo = uboBuffer.GetMappedPointer<UBO>();

	ubo->camPos = glm::vec4(cam->position, 1.0f);
	ubo->proj = cam->GetProjectionMatrix();
	ubo->view = cam->GetViewMatrix();
}

void DeferredPipeline::AddLight(const Light& light)
{
	LightBuffer* lights = lightBuffer.GetMappedPointer<LightBuffer>();
	lights->lights[lights->count++] = light;
}

DeferredPipeline::~DeferredPipeline()
{
	vgm::Delete(renderPass);
	vgm::Delete(rtgiView);
}