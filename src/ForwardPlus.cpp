#include "renderer/ForwardPlus.h"
#include "renderer/Vulkan.h"
#include "renderer/ComputeShader.h"

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

	vkDestroyBuffer(context.logicalDevice, cellBuffer, nullptr);
	vkFreeMemory(context.logicalDevice, cellMemory, nullptr);

	vkDestroyBuffer(context.logicalDevice, lightBuffer, nullptr);
	vkFreeMemory(context.logicalDevice, lightMemory, nullptr);

	vkDestroyBuffer(context.logicalDevice, matricesBuffer, nullptr);
	vkFreeMemory(context.logicalDevice, matricesMemory, nullptr);
}

void ForwardPlusPipeline::Allocate()
{
	VkDeviceSize size = cellWidth * cellHeight * sizeof(Cell) + sizeof(uint32_t) * 2;

	Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, cellBuffer, cellMemory);
	Vulkan::CreateBuffer(MAX_LIGHTS * sizeof(glm::vec3), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, lightBuffer, lightMemory);
	Vulkan::CreateBuffer(sizeof(Matrices), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, matricesBuffer, matricesMemory);

	const Vulkan::Context& context = Vulkan::GetContext();
	VkResult result = vkMapMemory(context.logicalDevice, matricesMemory, 0, sizeof(Matrices), 0, (void**)&matrices);
	CheckVulkanResult("Failed to map the forward+ matrices memory", result, vkMapMemory);

	uint32_t* dimensions = nullptr;
	result = vkMapMemory(context.logicalDevice, cellMemory, 0, sizeof(uint32_t) * 2, 0, (void**)&dimensions);
	CheckVulkanResult("Failed to map the forward+ cell memory", result, vkMapMemory);

	dimensions[0] = cellWidth;
	dimensions[1] = cellHeight;

	vkUnmapMemory(context.logicalDevice, cellMemory);
}

void ForwardPlusPipeline::CreateShader()
{
	computeShader = new ComputeShader("shaders/spirv/forwardPlus.comp.spv");

	computeShader->WriteToDescriptorBuffer(cellBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 0);
	computeShader->WriteToDescriptorBuffer(lightBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 1);
	computeShader->WriteToDescriptorBuffer(matricesBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 2);
}

void ForwardPlusPipeline::Execute(const Payload& payload, const std::vector<Object*>& objects)
{
	if (lightCount == 0)
		return;

	matrices->projection = payload.camera->GetProjectionMatrix();
	matrices->view = payload.camera->GetViewMatrix();

	vkCmdFillBuffer(payload.commandBuffer, cellBuffer, sizeof(uint32_t) * 2, VK_WHOLE_SIZE, 0);

	computeShader->Execute(payload.commandBuffer, lightCount, 1, 1);

	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.buffer = cellBuffer;
	barrier.size = VK_WHOLE_SIZE;

	vkCmdPipelineBarrier(payload.commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void ForwardPlusPipeline::AddLight(glm::vec3 pos)
{
	if (lightCount + 1 >= MAX_LIGHTS)
		throw std::runtime_error("Fatal error: upper light limit succeeded");

	const Vulkan::Context& context = Vulkan::GetContext();

	glm::vec3* lights = nullptr;
	VkDeviceSize offset = lightCount == 0 ? 0 : (lightCount - 1) * sizeof(glm::vec3);
	VkResult result = vkMapMemory(context.logicalDevice, lightMemory, offset, sizeof(glm::vec3), 0, (void**)&lights);
	CheckVulkanResult("Failed to map the forward+ light memory", result, vkMapMemory);

	lights[lightCount++] = pos;

	vkUnmapMemory(context.logicalDevice, lightMemory);
}