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

void DeferredPipeline::Start(const Payload& payload)
{
	std::vector<VkFormat> formats = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM };

	CreateRenderPass(formats);
	CreatePipelines(renderPass, payload.renderer->GetDefault3DRenderPass());

	framebuffer.Init(renderPass, payload.width, payload.height, formats); // 32 bit format takes a lot of performance compared to an 8 bit format
	//skyboxFramebuffer.Observe(framebuffer.GetViews()[1], framebuffer.GetDepthView(), payload.width, payload.height, framebuffer.GetRenderPass());

	uboBuffer.Init(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	uboBuffer.MapPermanently();

	lightBuffer.Init(sizeof(LightBuffer), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	lightBuffer.MapPermanently();

	const Material& mat = Mesh::materials[0];

	firstPipeline->BindBufferToName("ubo", uboBuffer.Get());
	secondPipeline->BindBufferToName("lights", lightBuffer.Get());

	const VkSampler& sampler = Renderer::defaultSampler;
	const std::vector<VkImageView> views = framebuffer.GetViews();

	constexpr VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	secondPipeline->BindImageToName("positionImage", views[0], sampler, layout);
	secondPipeline->BindImageToName("albedoImage", views[1], sampler, layout);
	secondPipeline->BindImageToName("normalImage", views[2], sampler, layout);
	secondPipeline->BindImageToName("metallicRoughnessAOImage", views[3], sampler, layout);

	if (Renderer::canRayTrace)
	{
		TLAS = TopLevelAccelerationStructure::Create({});
		
		VkWriteDescriptorSetAccelerationStructureKHR ASDescriptorInfo{};
		ASDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		ASDescriptorInfo.accelerationStructureCount = 1;
		ASDescriptorInfo.pAccelerationStructures = &TLAS->accelerationStructure;

		const std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT>& FIFsets = secondPipeline->GetAllDescriptorSets();

		for (const std::vector<VkDescriptorSet>& sets : FIFsets)
		{
			VkWriteDescriptorSetAccelerationStructureKHR ASDescriptorInfo{};
			ASDescriptorInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			ASDescriptorInfo.accelerationStructureCount = 1;
			ASDescriptorInfo.pAccelerationStructures = &TLAS->accelerationStructure;

			VkWriteDescriptorSet writeSet{};

			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			writeSet.pNext = &ASDescriptorInfo;
			writeSet.dstSet = sets[0];
			writeSet.descriptorCount = 1;
			writeSet.dstBinding = 4;

			vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, 1, &writeSet, 0, nullptr); // better to incorporate this into Pipeline
		}
	}

	//RayTracingPipeline test("shaders/spirv/rtgi.rgen.spv", "shaders/spirv/rtgi.rchit.spv", "shaders/spirv/rtgi.rmiss.spv");

	SetTextureBuffer();
}

void DeferredPipeline::CreateRenderPass(const std::vector<VkFormat>& formats)
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
	
	firstPipeline = new GraphicsPipeline(createInfo);

	createInfo.vertexShader   = "shaders/spirv/deferredSecond.vert.spv";
	createInfo.fragmentShader = Renderer::canRayTrace ? "shaders/spirv/deferredSecondRT.frag.spv" : "shaders/spirv/deferredSecond.frag.spv";
	createInfo.renderPass = secondPass;
	createInfo.noVertices = true;

	secondPipeline = new GraphicsPipeline(createInfo);
}

void DeferredPipeline::LoadSkybox(const std::string& path)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkCommandPool pool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
	VkCommandBuffer cmdBuffer = Vulkan::BeginSingleTimeCommands(pool);

	Skybox* brandnew = Skybox::ReadFromHDR(path, cmdBuffer);
	
	Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, cmdBuffer, pool);
	Vulkan::YieldCommandPool(ctx.graphicsIndex, pool);

	if (skybox != nullptr)
		delete skybox;
	skybox = brandnew;

	uint32_t width = framebuffer.GetWidth(), height = framebuffer.GetHeight();
	if (width != 0 && height != 0)
		skybox->Resize(width, height);

	skybox->targetView = framebuffer.GetViews()[1];
	skybox->depth = framebuffer.GetDepthView();
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

	//skybox->render

	cmdBuffer.EndRenderPass();

	if (skybox != nullptr)
		skybox->Draw(payload.commandBuffer, payload.camera);

	renderer->StartRenderPass(renderer->GetDefault3DRenderPass());

	secondPipeline->Bind(cmdBuffer);

	glm::vec3 camPos = payload.camera->position;

	secondPipeline->PushConstant(cmdBuffer, camPos, VK_SHADER_STAGE_FRAGMENT_BIT);

	cmdBuffer.Draw(6, 1, 0, 0);

	cmdBuffer.EndRenderPass();

	framebuffer.TransitionFromReadToWrite(cmdBuffer);
}

void DeferredPipeline::Resize(const Payload& payload)
{
	framebuffer.Resize(payload.width, payload.height);
	//skyboxFramebuffer.Observe(framebuffer.GetViews()[1], framebuffer.GetDepthView(), payload.width, payload.height, framebuffer.GetRenderPass());

	const VkSampler& sampler = Renderer::defaultSampler;
	const std::vector<VkImageView> views = framebuffer.GetViews();

	constexpr VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	secondPipeline->BindImageToName("positionImage", views[0], sampler, layout);
	secondPipeline->BindImageToName("albedoImage", views[1], sampler, layout);
	secondPipeline->BindImageToName("normalImage", views[2], sampler, layout);
	secondPipeline->BindImageToName("metallicRoughnessAOImage", views[3], sampler, layout);

	skybox->Resize(payload.width, payload.height);
	skybox->targetView = framebuffer.GetViews()[1];
	skybox->depth = framebuffer.GetDepthView();
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

void DeferredPipeline::Destroy()
{
	vgm::Delete(renderPass);

	if (skybox != nullptr)
		delete skybox;

	delete firstPipeline;
	delete secondPipeline;

	delete TLAS;

	framebuffer.~Framebuffer();

	lightBuffer.~Buffer();
	uboBuffer.~Buffer();
}