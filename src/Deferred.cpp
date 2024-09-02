#include "renderer/Deferred.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Renderer.h"
#include "renderer/DescriptorWriter.h"

#include "core/Object.h"
#include "core/Camera.h"

struct PushConstant
{
	glm::mat4 model;
	int materialID;
};

void DeferredPipeline::Start(const Payload& payload)
{
	std::vector<VkFormat> formats = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM };

	const Vulkan::Context& ctx = Vulkan::GetContext();
	renderPass = PipelineCreator::CreateRenderPass(ctx.physicalDevice, formats, PIPELINE_FLAG_CLEAR_ON_LOAD, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	firstPipeline  = new GraphicsPipeline("shaders/spirv/deferredFirst.vert.spv",  "shaders/spirv/deferredFirst.frag.spv",  PIPELINE_FLAG_CULL_BACK | PIPELINE_FLAG_FRONT_CCW, renderPass);
	secondPipeline = new GraphicsPipeline("shaders/spirv/deferredSecond.vert.spv", "shaders/spirv/deferredSecond.frag.spv", PIPELINE_FLAG_CULL_BACK | PIPELINE_FLAG_FRONT_CCW | PIPELINE_FLAG_NO_VERTEX, payload.renderer->GetDefault3DRenderPass());

	framebuffer.Init(renderPass, payload.width, payload.height, formats); // 32 bit format takes a lot of performance compared to an 8 bit format

	uboBuffer.Init(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	uboBuffer.MapPermanently();
	//ubo = uboBuffer.Map<UBO>();

	lightBuffer.Init(sizeof(LightBuffer), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	lightBuffer.MapPermanently();
	//lights = lightBuffer.Map<LightBuffer>();

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

	SetTextureBuffer();
}

void DeferredPipeline::Execute(const Payload& payload, const std::vector<Object*>& objects)
{
	UpdateTextureBuffer(); // temp !!!

	UpdateUBO(payload.camera);

	const VkCommandBuffer cmdBuffer = payload.commandBuffer;
	Renderer* renderer = payload.renderer;

	framebuffer.StartRenderPass(cmdBuffer);

	firstPipeline->Bind(cmdBuffer);

	VkBuffer vertexBuffer = Renderer::g_vertexBuffer.GetBufferHandle();
	VkDeviceSize offset = 0;

	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);
	vkCmdBindIndexBuffer(cmdBuffer, Renderer::g_indexBuffer.GetBufferHandle(), 0, VK_INDEX_TYPE_UINT16);

	PushConstant pushConstant{};
	for (Object* obj : objects)
	{
		pushConstant.model = obj->transform.GetModelMatrix();
		pushConstant.materialID = obj->mesh.materialIndex;

		vkCmdPushConstants(cmdBuffer, firstPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &pushConstant);

		uint32_t indexCount = static_cast<uint32_t>(obj->mesh.indices.size());
		uint32_t firstIndex = static_cast<uint32_t>(Renderer::g_indexBuffer.GetItemOffset(obj->mesh.indexMemory));
		uint32_t vertexOffset = static_cast<uint32_t>(Renderer::g_vertexBuffer.GetItemOffset(obj->mesh.vertexMemory));

		vkCmdDrawIndexed(cmdBuffer, indexCount, 1, firstIndex, vertexOffset, 0);
	}

	vkCmdEndRenderPass(cmdBuffer);

	renderer->StartRenderPass(cmdBuffer, renderer->GetDefault3DRenderPass());

	secondPipeline->Bind(cmdBuffer);

	glm::vec3 camPos = payload.camera->position;

	vkCmdPushConstants(cmdBuffer, secondPipeline->GetLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(camPos), &camPos);

	vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

	vkCmdEndRenderPass(cmdBuffer);

	framebuffer.TransitionFromReadToWrite(cmdBuffer);
}

void DeferredPipeline::Resize(const Payload& payload)
{
	framebuffer.Resize(payload.width, payload.height);

	const VkSampler& sampler = Renderer::defaultSampler;
	const std::vector<VkImageView> views = framebuffer.GetViews();

	constexpr VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	secondPipeline->BindImageToName("positionImage", views[0], sampler, layout);
	secondPipeline->BindImageToName("albedoImage", views[1], sampler, layout);
	secondPipeline->BindImageToName("normalImage", views[2], sampler, layout);
	secondPipeline->BindImageToName("metallicRoughnessAOImage", views[3], sampler, layout);
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
	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkDestroyRenderPass(ctx.logicalDevice, renderPass, nullptr);

	delete firstPipeline;
	delete secondPipeline;

	framebuffer.~Framebuffer();

	lightBuffer.~Buffer();
	uboBuffer.~Buffer();
}