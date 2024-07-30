#include "renderer/ForwardPlus.h"
#include "renderer/Vulkan.h"
#include "renderer/ComputeShader.h"
#include "renderer/GraphicsPipeline.h"

#include "core/Camera.h"

void ForwardPlusPipeline::Start(const Payload& payload)
{
	Allocate();
	CreateShader();
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
	cellBuffer.Init(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, flags);
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
	graphicsPipeline = new GraphicsPipeline("shaders/spirv/triangle.vert.spv", "shaders/spirv/triangle.frag.spv", PIPELINE_FLAG_CULL_BACK | PIPELINE_FLAG_FRONT_CCW, renderPass);

	computeShader->WriteToDescriptorBuffer(cellBuffer.Get(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 0);
	computeShader->WriteToDescriptorBuffer(lightBuffer.Get(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 1);
	computeShader->WriteToDescriptorBuffer(matricesBuffer.Get(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 2);
}

void ForwardPlusPipeline::Execute(const Payload& payload, const std::vector<Object*>& objects)
{
	if (lightCount == 0)
		return;

	matrices->projection = payload.camera->GetProjectionMatrix();
	matrices->view = payload.camera->GetViewMatrix();

	vkCmdFillBuffer(payload.commandBuffer, cellBuffer.Get(), sizeof(uint32_t) * 2, VK_WHOLE_SIZE, 0);

	computeShader->Execute(payload.commandBuffer, lightCount, 1, 1);

	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.buffer = cellBuffer.Get();
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(payload.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
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