#include "renderer/ForwardPlus.h"
#include "renderer/Vulkan.h"
#include "renderer/ComputeShader.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/Renderer.h"

#include "core/Camera.h"
#include "core/Object.h"

void ForwardPlusPipeline::Start(const Payload& payload)
{
	Allocate();
	CreateShader();
	PrepareGraphicsPipeline();
}

void ForwardPlusPipeline::Destroy()
{
	const Vulkan::Context& context = Vulkan::GetContext();

	delete computeShader;
	delete graphicsPipeline;
}

void ForwardPlusPipeline::Allocate()
{
	VkDeviceSize size = cellWidth * cellHeight * sizeof(Cell) + sizeof(uint32_t) * 2;

	VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	cellBuffer.Init(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	lightBuffer.Init(MAX_LIGHTS * sizeof(glm::vec3), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, flags);
	matricesBuffer.Init(sizeof(Matrices), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, flags);

	matrices = matricesBuffer.Map<Matrices>();

	uint32_t* dimensions = cellBuffer.Map<uint32_t>(0, sizeof(uint32_t) * 2);

	dimensions[0] = cellWidth;
	dimensions[1] = cellHeight;

	cellBuffer.Unmap();
}

void ForwardPlusPipeline::CreateShader()
{
	computeShader = new ComputeShader("shaders/spirv/forwardPlus.comp.spv");

	computeShader->WriteToDescriptorBuffer(cellBuffer.Get(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 0);
	computeShader->WriteToDescriptorBuffer(lightBuffer.Get(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 1);
	computeShader->WriteToDescriptorBuffer(matricesBuffer.Get(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 2);
}

void ForwardPlusPipeline::Execute(const Payload& payload, const std::vector<Object*>& objects)
{
	if (lightCount == 0)
		return;

	const VkCommandBuffer cmdBuffer = payload.commandBuffer;

	matrices->projection = payload.camera->GetProjectionMatrix();
	matrices->view = payload.camera->GetViewMatrix();

	vkCmdFillBuffer(cmdBuffer, cellBuffer.Get(), sizeof(uint32_t) * 2, VK_WHOLE_SIZE, 0);

	computeShader->Execute(cmdBuffer, lightCount, 1, 1);

	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.buffer = cellBuffer.Get();
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
	
	payload.renderer->StartRenderPass(cmdBuffer, renderPass);

	{
		UpdateUniformBuffer(payload.camera, payload.renderer->GetInternalWidth(), payload.renderer->GetInternalHeight());

		VkBuffer vertexBuffer = Renderer::g_vertexBuffer.GetBufferHandle();
		VkDeviceSize offset = 0;

		graphicsPipeline->Bind(cmdBuffer);

		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &vertexBuffer, &offset);
		vkCmdBindIndexBuffer(cmdBuffer, Renderer::g_indexBuffer.GetBufferHandle(), 0, VK_INDEX_TYPE_UINT16);

		for (Object* obj : objects)
		{
			glm::mat4 model = obj->transform.GetModelMatrix();
			vkCmdPushConstants(cmdBuffer, graphicsPipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(model), &model);

			uint32_t indexCount = static_cast<uint32_t>(obj->mesh.indices.size());
			uint32_t firstIndex = static_cast<uint32_t>(Renderer::g_indexBuffer.GetItemOffset(obj->mesh.indexMemory));
			uint32_t vertexOffset = static_cast<uint32_t>(Renderer::g_vertexBuffer.GetItemOffset(obj->mesh.vertexMemory));

			vkCmdDrawIndexed(cmdBuffer, indexCount, 1, firstIndex, vertexOffset, 0);
		}
	}
	payload.renderer->EndRenderPass(cmdBuffer);
}

void ForwardPlusPipeline::AddLight(glm::vec3 pos)
{
	if (lightCount + 1 >= MAX_LIGHTS)
		throw std::runtime_error("Fatal error: upper light limit succeeded");

	const Vulkan::Context& context = Vulkan::GetContext();

	VkDeviceSize offset = lightCount == 0 ? 0 : (lightCount - 1) * sizeof(glm::vec3);
	glm::vec3* lights = lightBuffer.Map<glm::vec3>(offset, sizeof(glm::vec3));

	lights[lightCount++] = pos;

	lightBuffer.Unmap();
}

void ForwardPlusPipeline::PrepareGraphicsPipeline()
{
	DescriptorWriter* writer = DescriptorWriter::Get();
	graphicsPipeline = new GraphicsPipeline("shaders/spirv/triangle.vert.spv", "shaders/spirv/triangle.frag.spv", PIPELINE_FLAG_CULL_BACK | PIPELINE_FLAG_FRONT_CCW, renderPass);

	constexpr VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	modelBuffer.Init(sizeof(glm::mat4) * Renderer::MAX_MESHES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, flags);
	uniformBuffer.Init(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, flags);

	modelBufferMapped = modelBuffer.Map<ModelData>();
	uniformBufferMapped = uniformBuffer.Map<UniformBufferObject>();

	{
		const std::vector<VkDescriptorSet>& sets = graphicsPipeline->GetDescriptorSets();

		writer->WriteBuffer(sets[0], uniformBuffer.Get(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0);
		writer->WriteBuffer(sets[0], modelBuffer.Get(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
		writer->WriteBuffer(sets[0], cellBuffer.Get(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3);
		writer->WriteBuffer(sets[0], lightBuffer.Get(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4);
	}
}

void ForwardPlusPipeline::UpdateUniformBuffer(Camera* cam, uint32_t width, uint32_t height)
{
	uniformBufferMapped->cameraPos = cam->position;
	uniformBufferMapped->projection = cam->GetProjectionMatrix();
	uniformBufferMapped->view = cam->GetViewMatrix();
	uniformBufferMapped->width = width;
	uniformBufferMapped->height = height;
}