#include <array>

#include "renderer/Intro.h"
#include "renderer/Renderer.h"
#include "renderer/Swapchain.h"
#include "renderer/Texture.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/PipelineCreator.h"
#include "renderer/Vulkan.h"

#include "io/IO.h"

struct Intro::Timer
{
	float completionPercentage;
};

void Intro::Create(Swapchain* swapchain, std::string imagePath)
{
	this->swapchain = swapchain;
	this->texture = new Texture(imagePath);
	this->texture->AwaitGeneration();

	CreateRenderPass();
	CreatePipeline();

	uniformBuffer.Init(sizeof(Timer), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	pTimer = uniformBuffer.Map<Timer>();

	pipeline->BindBufferToName("timer", uniformBuffer.Get());
	pipeline->BindImageToName("image", texture->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	DescriptorWriter::Write();
}

void Intro::CreateRenderPass()
{
	RenderPassBuilder builder(swapchain->format);

	builder.SetInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	builder.SetFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	renderPass = builder.Build();
}

void Intro::CreatePipeline()
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/spirv/intro.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/intro.frag.spv";

	createInfo.renderPass = renderPass;
	createInfo.noVertices = true;

	pipeline = new GraphicsPipeline(createInfo);
}

void Intro::WriteDataToBuffer(float timeElapsed)
{
	if (timeElapsed < fadeInOutTime) // fade in
		pTimer->completionPercentage = timeElapsed / fadeInOutTime;
	else if (timeElapsed > maxSeconds - fadeInOutTime) // fade out
		pTimer->completionPercentage = 1 - (timeElapsed - (maxSeconds - fadeInOutTime)) / fadeInOutTime;
	
	if (pTimer->completionPercentage > 1) pTimer->completionPercentage = 1;
}

void Intro::RecordCommandBuffer(CommandBuffer commandBuffer, uint32_t imageIndex)
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

	commandBuffer.BeginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	pipeline->Bind(commandBuffer.Get());

	VkViewport viewport{};
	Vulkan::PopulateDefaultViewport(viewport, swapchain->extent);
	commandBuffer.SetViewport(viewport);

	VkRect2D scissor{};
	Vulkan::PopulateDefaultScissors(scissor, swapchain->extent);
	commandBuffer.SetScissor(scissor);

	commandBuffer.Draw(6, 1, 0, 0);

	commandBuffer.EndRenderPass();
}

void Intro::Destroy()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	vkDeviceWaitIdle(ctx.logicalDevice);

	vkDestroyRenderPass(ctx.logicalDevice, renderPass, nullptr);

	delete pipeline;
	delete texture;
}