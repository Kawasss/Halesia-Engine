#include "renderer/ForwardPlus.h"
#include "renderer/Vulkan.h"

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
	delete computeShader;

	const Vulkan::Context& context = Vulkan::GetContext();

	vkDestroyBuffer(context.logicalDevice, cellBuffer, nullptr);
	vkFreeMemory(context.logicalDevice, cellMemory, nullptr);
}

void ForwardPlusRenderer::Allocate()
{
	VkDeviceSize size = cellWidth * cellHeight * cellDepth * sizeof(Cell);
	Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, cellBuffer, cellMemory);
}

void ForwardPlusRenderer::CreateShader()
{

}