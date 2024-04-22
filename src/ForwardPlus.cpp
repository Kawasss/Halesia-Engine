#include "renderer/ForwardPlus.h"
#include "renderer/Vulkan.h"
#include "renderer/ComputeShader.h"

#include "core/Camera.h"

ForwardPlusRenderer::ForwardPlusRenderer()
{
	Allocate();
	CreateShader();
}

ForwardPlusRenderer::~ForwardPlusRenderer()
{
	Destroy();
}

void ForwardPlusRenderer::Destroy()
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

void ForwardPlusRenderer::Allocate()
{
	VkDeviceSize size = cellWidth * cellHeight * cellDepth * sizeof(Cell) + sizeof(uint32_t) * 3;

	Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, cellBuffer, cellMemory);
	Vulkan::CreateBuffer(MAX_LIGHTS * sizeof(glm::vec3), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, lightBuffer, lightMemory);
	Vulkan::CreateBuffer(sizeof(Matrices), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, matricesBuffer, matricesMemory);

	const Vulkan::Context& context = Vulkan::GetContext();
	VkResult result = vkMapMemory(context.logicalDevice, matricesMemory, 0, sizeof(Matrices), 0, (void**)&matrices);
	CheckVulkanResult("Failed to map the forward+ matrices memory", result, vkMapMemory);

	uint32_t* dimensions = nullptr;
	result = vkMapMemory(context.logicalDevice, cellMemory, 0, sizeof(uint32_t) * 3, 0, (void**)&dimensions);
	CheckVulkanResult("Failed to map the forward+ cell memory", result, vkMapMemory);

	dimensions[0] = cellDepth;
	dimensions[1] = cellWidth;
	dimensions[2] = cellHeight;

	vkUnmapMemory(context.logicalDevice, cellMemory);
}

void ForwardPlusRenderer::CreateShader()
{
	computeShader = new ComputeShader("shaders/spirv/forwardPlus.comp.spv");

	computeShader->WriteToDescriptorBuffer(cellBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 0);
	computeShader->WriteToDescriptorBuffer(lightBuffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, 1);
	computeShader->WriteToDescriptorBuffer(matricesBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 2);
}

void ForwardPlusRenderer::Draw(VkCommandBuffer commandBuffer, Camera* camera)
{
	const Vulkan::Context& context = Vulkan::GetContext();

	void* cells = nullptr;
	uint64_t size = cellWidth * cellHeight * cellDepth * sizeof(Cell);
	VkResult result = vkMapMemory(context.logicalDevice, cellMemory, sizeof(uint32_t) * 3, size, 0, &cells);
	CheckVulkanResult("Failed to map the forward+ cell memory", result, vkMapMemory);

	memset(cells, 0, size); // reset Cell::lightCount for every cell
	vkUnmapMemory(context.logicalDevice, cellMemory);

	if (lightCount == 0)
		return;

	matrices->projection = camera->GetProjectionMatrix();
	matrices->view = camera->GetViewMatrix();

	computeShader->Execute(commandBuffer, lightCount, 1, 1);
}

void ForwardPlusRenderer::AddLight(glm::vec3 pos)
{
	if (lightCount + 1 >= MAX_LIGHTS)
		throw std::runtime_error("Fatal error: upper light limit succeeded");

	const Vulkan::Context& context = Vulkan::GetContext();

	glm::vec3* lights = nullptr;
	VkDeviceSize offset = (lightCount - 1) * sizeof(glm::vec3);
	VkResult result = vkMapMemory(context.logicalDevice, lightMemory, offset, sizeof(glm::vec3), 0, (void**)&lights);
	CheckVulkanResult("Failed to map the forward+ light memory", result, vkMapMemory);

	lights[lightCount++] = pos;

	vkUnmapMemory(context.logicalDevice, lightMemory);
}