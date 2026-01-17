module;

#include <vulkan/vulkan.h>

#include "glm.h"

#include "renderer/VulkanAPIError.h"
#include "renderer/Renderer.h"
#include "renderer/Texture.h"
#include "renderer/Vulkan.h"

module Renderer.HdrConverter;

import std;

import Renderer.VulkanGarbageManager;
import Renderer.DescriptorWriter;
import Renderer.PipelineCreator;
import Renderer.Skybox;
import Renderer.GraphicsPipeline;

struct PushConstantConverter
{
	glm::mat4 projection;
	glm::mat4 view;
};

VkRenderPass renderPass;
VkFramebuffer framebuffer;
GraphicsPipeline* pipeline = nullptr;

const std::array<glm::mat4, 6> views = // sadly cant be constexpr
{
	glm::lookAt(glm::vec3(0), glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0)),
	glm::lookAt(glm::vec3(0), glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0)),
	glm::lookAt(glm::vec3(0), glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1)),
	glm::lookAt(glm::vec3(0), glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1)),
	glm::lookAt(glm::vec3(0), glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0)),
	glm::lookAt(glm::vec3(0), glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0)),
};

static void CreateRenderPass()
{
	RenderPassBuilder builder(VK_FORMAT_R8G8B8A8_SRGB);

	builder.SetInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	builder.SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	builder.DisableDepth(true);
	builder.ClearOnLoad(true);

	renderPass = builder.Build();
}

static void CreatePipeline()
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/uncompiled/cubemapConv.vert";
	createInfo.fragmentShader = "shaders/uncompiled/cubemapConv.frag";

	createInfo.renderPass = renderPass;
	createInfo.noVertices = true;
	createInfo.noCulling  = true;
	createInfo.noDepth    = true;

	pipeline = new GraphicsPipeline(createInfo);
}

static void CreateFramebuffer()
{
	const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

	VkFramebufferAttachmentImageInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageInfo.width = Skybox::WIDTH;
	imageInfo.height = Skybox::HEIGHT;
	imageInfo.layerCount = 1;
	imageInfo.viewFormatCount = 1;
	imageInfo.pViewFormats = &format;

	VkFramebufferAttachmentsCreateInfo attachInfo{};
	attachInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
	attachInfo.attachmentImageInfoCount = 1;
	attachInfo.pAttachmentImageInfos = &imageInfo;

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
	createInfo.pNext = &attachInfo;
	createInfo.width = Skybox::WIDTH;
	createInfo.height = Skybox::HEIGHT;
	createInfo.layers = 1;
	createInfo.attachmentCount = 1;
	createInfo.renderPass = renderPass;

	VkResult result = vkCreateFramebuffer(Vulkan::GetContext().logicalDevice, &createInfo, nullptr, &framebuffer);
	CheckVulkanResult("Failed to create framebuffer", result);
}

void HdrConverter::Start()
{
	CreateRenderPass();
	CreatePipeline();
	CreateFramebuffer();
}

void HdrConverter::End()
{
	vgm::Delete(renderPass);
	vgm::Delete(framebuffer);

	delete pipeline;
}

void HdrConverter::ConvertTextureIntoCubemap(const CommandBuffer& cmdBuffer, const Texture* texture, Cubemap* cubemap)
{
	Renderer::SetViewport(cmdBuffer, { Skybox::WIDTH, Skybox::HEIGHT });
	Renderer::SetScissors(cmdBuffer, { Skybox::WIDTH, Skybox::HEIGHT });

	pipeline->BindImageToName("equirectangularMap", texture->view, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	DescriptorWriter::Write();

	PushConstantConverter push{};
	push.projection = glm::perspective(glm::pi<float>() * 0.5f, 1.0f, 0.01f, 1000.0f);
	push.projection[1][1] *= -1;

	pipeline->Bind(cmdBuffer);

	cmdBuffer.SetCullMode(VK_CULL_MODE_BACK_BIT);

	for (int i = 0; i < views.size(); i++)
	{
		int actual = i;
		if (i == 3) // reverse the up and down texture (ugly, maybe switch the view matrices from views around)
			actual = 2;
		else if (i == 2)
			actual = 3;

		push.view = views[actual];

		pipeline->PushConstant(cmdBuffer, push, VK_SHADER_STAGE_VERTEX_BIT);

		std::array<VkClearValue, 1> clearColors{};
		clearColors[0].color = { 1, 0, 0, 1 };

		VkRenderPassAttachmentBeginInfo attachInfo{};
		attachInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO;
		attachInfo.attachmentCount = 1;
		attachInfo.pAttachments = &cubemap->layerViews[i];

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = &attachInfo;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = framebuffer;
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = { Skybox::WIDTH, Skybox::HEIGHT };
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
		renderPassBeginInfo.pClearValues = clearColors.data();

		cmdBuffer.BeginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		cmdBuffer.Draw(36, 1, 0, 0);
		cmdBuffer.EndRenderPass();
	}
}