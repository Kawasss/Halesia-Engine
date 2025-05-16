#include "renderer/ForwardPlus.h"
#include "renderer/Vulkan.h"
#include "renderer/ComputeShader.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Renderer.h"
#include "renderer/Texture.h"
#include "renderer/Light.h"

#include "core/Camera.h"
#include "core/MeshObject.h"

struct PushConstant
{
	glm::mat4 model;
	int materialID;
};

void ForwardPlusPipeline::Start(const Payload& payload)
{
	Allocate();
	CreateShader();
	PrepareGraphicsPipeline();
}

void ForwardPlusPipeline::Allocate()
{
	VkDeviceSize size = cellWidth * cellHeight * sizeof(Cell) + sizeof(uint32_t) * 2;

	VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	cellBuffer.Init(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	matricesBuffer.Init(sizeof(Matrices), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, flags);

	matrices = matricesBuffer.Map<Matrices>();

	uint32_t* dimensions = cellBuffer.Map<uint32_t>(0, sizeof(uint32_t) * 2);

	dimensions[0] = cellWidth;
	dimensions[1] = cellHeight;

	cellBuffer.Unmap();
}

void ForwardPlusPipeline::CreateShader()
{
	computeShader = std::make_unique<ComputeShader>("shaders/spirv/forwardPlus.comp.spv");

	computeShader->BindBufferToName("cells", cellBuffer.Get());
	computeShader->BindBufferToName("matrices", matricesBuffer.Get());
}

void ForwardPlusPipeline::Execute(const Payload& payload, const std::vector<MeshObject*>& objects)
{
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	if (payload.renderer->GetLightCount() > 0)
	{
		ComputeCells(cmdBuffer, payload.renderer->GetLightCount(), payload.camera);
	}

	UpdateBindlessTextures();

	payload.renderer->StartRenderPass(renderPass);

	DrawObjects(cmdBuffer, objects, payload.camera, payload.renderer->GetInternalWidth(), payload.renderer->GetInternalHeight());

	cmdBuffer.EndRenderPass();
}

void ForwardPlusPipeline::ComputeCells(CommandBuffer commandBuffer, uint32_t lightCount, Camera* camera)
{
	matrices->projection = camera->GetProjectionMatrix();
	matrices->view = camera->GetViewMatrix();

	commandBuffer.BeginDebugUtilsLabel(__FUNCTION__);

	cellBuffer.Fill(commandBuffer.Get(), 0, sizeof(uint32_t) * 2);

	computeShader->Execute(commandBuffer, lightCount, 1, 1);

	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.buffer = cellBuffer.Get();
	barrier.size = VK_WHOLE_SIZE;

	commandBuffer.BufferMemoryBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier);

	commandBuffer.EndDebugUtilsLabel();
}

void ForwardPlusPipeline::DrawObjects(CommandBuffer commandBuffer, const std::vector<MeshObject*>& objects, Camera* camera, uint32_t width, uint32_t height, glm::mat4 customProj)
{
	UpdateUniformBuffer(camera, customProj, width, height);

	commandBuffer.BeginDebugUtilsLabel(__FUNCTION__);

	graphicsPipeline->Bind(commandBuffer);

	commandBuffer.BindVertexBuffer(Renderer::g_vertexBuffer.GetBufferHandle(), 0);
	commandBuffer.BindIndexBuffer(Renderer::g_indexBuffer.GetBufferHandle(), 0, VK_INDEX_TYPE_UINT16);

	PushConstant pushConstant{};
	for (MeshObject* obj : objects)
	{
		pushConstant.model      = obj->transform.GetModelMatrix();
		pushConstant.materialID = obj->mesh.GetMaterialIndex();

		commandBuffer.PushConstants(graphicsPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstant), &pushConstant);

		uint32_t indexCount   = static_cast<uint32_t>(obj->mesh.indices.size());
		uint32_t firstIndex   = static_cast<uint32_t>(Renderer::g_indexBuffer.GetItemOffset(obj->mesh.indexMemory));
		uint32_t vertexOffset = static_cast<uint32_t>(Renderer::g_vertexBuffer.GetItemOffset(obj->mesh.vertexMemory));

		commandBuffer.DrawIndexed(indexCount, 1, firstIndex, vertexOffset, 0);
	}
	commandBuffer.EndDebugUtilsLabel();
}

void ForwardPlusPipeline::PrepareGraphicsPipeline()
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/spirv/triangle.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/triangle.frag.spv";
	createInfo.renderPass     = renderPass;

	graphicsPipeline = std::make_unique<GraphicsPipeline>(createInfo);

	uniformBuffer.Init(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	uniformBufferMapped = uniformBuffer.Map<UniformBufferObject>();

	graphicsPipeline->BindBufferToName("ubo", uniformBuffer.Get());
	graphicsPipeline->BindBufferToName("cells", cellBuffer.Get());

	VkDescriptorImageInfo imageInfo{}; // prepare the texture buffer
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = VK_NULL_HANDLE;
	imageInfo.sampler = Renderer::defaultSampler;

	std::vector<VkDescriptorImageInfo> imageInfos(500, imageInfo);

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.dstSet = graphicsPipeline->GetDescriptorSets()[0];
	writeSet.pImageInfo = imageInfos.data();
	writeSet.descriptorCount = 500;
	writeSet.dstArrayElement = 0;
	writeSet.dstBinding = 5;

	vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, 1, &writeSet, 0, nullptr);
}

void ForwardPlusPipeline::UpdateBindlessTextures()
{
	constexpr size_t MAX_PROCESSED_COUNT = 5;

	const size_t pbrSize = Material::pbrTextures.size();
	const size_t maxSize = pbrSize * MAX_PROCESSED_COUNT;

	std::vector<VkDescriptorImageInfo> imageInfos(maxSize);
	std::vector<VkWriteDescriptorSet>  writeSets(maxSize);

	int processedCount = 0;

	if (processedMats.size() < Mesh::materials.size())
		processedMats.resize(Mesh::materials.size());

	for (int i = 0; i < Mesh::materials.size(); i++)
	{
		if (processedCount >= MAX_PROCESSED_COUNT)
			break;

		if (processedMats[i] == Mesh::materials[i].handle)
			continue;

		for (int j = 0; j < pbrSize; j++)
		{
			size_t index = i * pbrSize + j;

			VkDescriptorImageInfo& imageInfo = imageInfos[index];
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = Mesh::materials[i][Material::pbrTextures[j]]->imageView;
			imageInfo.sampler = Renderer::defaultSampler;

			VkWriteDescriptorSet& writeSet = writeSets[index];
			writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writeSet.descriptorCount = 1;
			writeSet.dstArrayElement = index;
			writeSet.dstBinding = 5;
			writeSet.dstSet = graphicsPipeline->GetDescriptorSets()[0];
			writeSet.pImageInfo = &imageInfos[index];
		}
		processedMats.push_back(Mesh::materials[i].handle);
		processedCount++;
	}

	if (processedCount > 0)
		vkUpdateDescriptorSets(Vulkan::GetContext().logicalDevice, static_cast<uint32_t>(processedCount * pbrSize), writeSets.data(), 0, nullptr);
}

void ForwardPlusPipeline::UpdateUniformBuffer(Camera* cam, glm::mat4 proj, uint32_t width, uint32_t height)
{
	uniformBufferMapped->cameraPos = cam->position;
	uniformBufferMapped->projection = proj == glm::mat4(0) ? cam->GetProjectionMatrix() : proj;
	uniformBufferMapped->view = cam->GetViewMatrix();
	uniformBufferMapped->width = width;
	uniformBufferMapped->height = height;
}