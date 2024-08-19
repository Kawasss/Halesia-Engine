#include <array>

#include "renderer/Intro.h"
#include "renderer/Renderer.h"
#include "renderer/Swapchain.h"
#include "renderer/Texture.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/PipelineCreator.h"

#include "io/IO.h"

struct Intro::Timer
{
	float completionPercentage;
};

void Intro::Create(Swapchain* swapchain, std::string imagePath)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	this->swapchain = swapchain;
	this->texture = new Texture(imagePath);
	this->texture->AwaitGeneration();

	renderPass = PipelineCreator::CreateRenderPass(ctx.physicalDevice, swapchain->format, PIPELINE_FLAG_NONE, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	// pipeline

	pipeline = new GraphicsPipeline("shaders/spirv/intro.vert.spv", "shaders/spirv/intro.frag.spv", PIPELINE_FLAG_NO_VERTEX, renderPass);

	// uniform buffer

	uniformBuffer.Init(sizeof(Timer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	pTimer = uniformBuffer.Map<Timer>();

	pipeline->BindBufferToName("timer", uniformBuffer.Get());
	pipeline->BindImageToName("image", texture->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	DescriptorWriter::Get()->Write();
}

void Intro::WriteDataToBuffer(float timeElapsed)
{
	if (timeElapsed < fadeInOutTime) // fade in
		pTimer->completionPercentage = timeElapsed / fadeInOutTime;
	else if (timeElapsed > maxSeconds - fadeInOutTime) // fade out
		pTimer->completionPercentage = 1 - (timeElapsed - (maxSeconds - fadeInOutTime)) / fadeInOutTime;
	
	if (pTimer->completionPercentage > 1) pTimer->completionPercentage = 1;
}

void Intro::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkRenderPassBeginInfo renderPassBeginInfo{};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = swapchain->framebuffers[imageIndex];
	renderPassBeginInfo.renderArea.offset = { 0, 0 };
	renderPassBeginInfo.renderArea.extent = swapchain->extent;

	std::array<VkClearValue, 2> clearColors{};
	clearColors[0].color = { 0, 0, 0, 1 };
	clearColors[1].depthStencil = { 1, 0 };

	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
	renderPassBeginInfo.pClearValues = clearColors.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	pipeline->Bind(commandBuffer);

	VkViewport viewport{};
	Vulkan::PopulateDefaultViewport(viewport, swapchain->extent);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	Vulkan::PopulateDefaultScissors(scissor, swapchain->extent);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdDraw(commandBuffer, 6, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);
}

void Intro::Destroy()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	vkDeviceWaitIdle(ctx.logicalDevice);

	vkDestroyRenderPass(ctx.logicalDevice, renderPass, nullptr);

	delete pipeline;
	delete texture;
}