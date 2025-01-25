#include <memory>

#include "glm.h"

#include "core/Camera.h"

#include "renderer/Skybox.h"
#include "renderer/Texture.h"
#include "renderer/HdrConverter.h"
#include "renderer/CommandBuffer.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/PipelineCreator.h"
#include "renderer/GarbageManager.h"
#include "renderer/Renderer.h"
#include "renderer/Vulkan.h"

struct PushConstantSkybox
{
	glm::mat4 view;
	glm::mat4 projection;
};

Skybox* Skybox::ReadFromHDR(const std::string& path, const CommandBuffer& cmdBuffer)
{
	Skybox* ret = new Skybox();

	std::unique_ptr<Texture> flat = std::make_unique<Texture>(path, false);
	flat->AwaitGeneration();

	ret->cubemap = new Cubemap(1024, 1024);

	HdrConverter::ConvertTextureIntoCubemap(cmdBuffer, flat.get(), ret->cubemap);

	ret->pipeline->BindImageToName("skybox", ret->cubemap->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	return ret;
}

Skybox::Skybox()
{
	RenderPassBuilder builder(VK_FORMAT_R8G8B8A8_UNORM);

	builder.SetInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	builder.SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	builder.ClearOnLoad(false);
	builder.DontClearDepth(true);

	renderPass = builder.Build();

	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/spirv/skybox.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/skybox.frag.spv";

	createInfo.renderPass = renderPass;
	createInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	createInfo.noVertices = true;
	createInfo.noCulling  = true;
	createInfo.writeDepth = false;

	pipeline = new GraphicsPipeline(createInfo);
}

void Skybox::CreateFramebuffer(uint32_t width, uint32_t height)
{
	constexpr VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	const VkFormat depthFormat = Vulkan::GetContext().physicalDevice.GetDepthFormat();

	VkFramebufferAttachmentImageInfo imageInfos[2]{};

	VkFramebufferAttachmentImageInfo& imageInfo = imageInfos[0];
	imageInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageInfo.width = width;
	imageInfo.height = height;
	imageInfo.layerCount = 1;
	imageInfo.viewFormatCount = 1;
	imageInfo.pViewFormats = &format;

	VkFramebufferAttachmentImageInfo& depthInfo = imageInfos[1];
	depthInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
	depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthInfo.width = width;
	depthInfo.height = height;
	depthInfo.layerCount = 1;
	depthInfo.viewFormatCount = 1;
	depthInfo.pViewFormats = &depthFormat;

	VkFramebufferAttachmentsCreateInfo attachInfo{};
	attachInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENTS_CREATE_INFO;
	attachInfo.attachmentImageInfoCount = 2;
	attachInfo.pAttachmentImageInfos = imageInfos;

	VkFramebufferCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT;
	createInfo.pNext = &attachInfo;
	createInfo.width = width;
	createInfo.height = height;
	createInfo.layers = 1;
	createInfo.attachmentCount = 2;
	createInfo.renderPass = renderPass;

	vkCreateFramebuffer(Vulkan::GetContext().logicalDevice, &createInfo, nullptr, &framebuffer);
}

void Skybox::Resize(uint32_t width, uint32_t height)
{
	this->width  = width;
	this->height = height;

	if (framebuffer != VK_NULL_HANDLE)
		vgm::Delete(framebuffer);

	CreateFramebuffer(width, height);
}

void Skybox::Draw(const CommandBuffer& cmdBuffer, Camera* camera)
{
	VkImageView targets[2]{ targetView, depth };

	VkClearValue clearValues[2]{};
	clearValues[0].color = { 1, 1, 1, 1 };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassAttachmentBeginInfo attachInfo{};
	attachInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO;
	attachInfo.attachmentCount = 2;
	attachInfo.pAttachments = targets;

	VkRenderPassBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.pNext = &attachInfo;
	beginInfo.renderPass = renderPass;
	beginInfo.pClearValues = clearValues;
	beginInfo.clearValueCount = 2;
	beginInfo.framebuffer = framebuffer;
	beginInfo.renderArea.extent = { width, height };

	cmdBuffer.BeginRenderPass(beginInfo, VK_SUBPASS_CONTENTS_INLINE);

	PushConstantSkybox push{};
	push.projection = camera->GetProjectionMatrix();
	push.view = glm::mat4(glm::mat3(camera->GetViewMatrix()));

	pipeline->Bind(cmdBuffer);
	pipeline->PushConstant(cmdBuffer, push, VK_SHADER_STAGE_VERTEX_BIT);

	cmdBuffer.Draw(36, 1, 0, 0);

	cmdBuffer.EndRenderPass();
}

void Skybox::Destroy()
{
	vgm::Delete(renderPass);
	vgm::Delete(framebuffer);

	delete pipeline;
	delete cubemap;
}