#include <memory>

#include "renderer/Skybox.h"
#include "renderer/Texture.h"
#include "renderer/HdrConverter.h"
#include "renderer/CommandBuffer.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/PipelineCreator.h"
#include "renderer/GarbageManager.h"
#include "renderer/Vulkan.h"

GraphicsPipeline* pipeline = nullptr; // find a way to make only ONE pipeline exist for all skyboxes !! (this reference counting is bad !!!!)
VkRenderPass renderPass;
int pipelineRefCount = 0;

Skybox* Skybox::ReadFromHDR(const std::string& path, const CommandBuffer& cmdBuffer)
{
	Skybox* ret = new Skybox();

	std::unique_ptr<Texture> flat = std::make_unique<Texture>(path, false);
	flat->AwaitGeneration();

	ret->cubemap = new Cubemap(1024, 1024);

	HdrConverter::ConvertTextureIntoCubemap(cmdBuffer, flat.get(), ret->cubemap);

	return ret;
}

Skybox::Skybox()
{
	pipelineRefCount++;
	if (pipeline != nullptr)
		return;

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
	createInfo.noVertices = true;
	createInfo.noCulling  = true;

	pipeline = new GraphicsPipeline(createInfo);
}

void Skybox::CreateFramebuffer()
{
	constexpr VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

	VkFramebufferAttachmentImageInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_ATTACHMENT_IMAGE_INFO;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageInfo.width = 1024;
	imageInfo.height = 1024;
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
	createInfo.width = 1024;
	createInfo.height = 1024;
	createInfo.layers = 1;
	createInfo.attachmentCount = 1;
	createInfo.renderPass = renderPass;

	vkCreateFramebuffer(Vulkan::GetContext().logicalDevice, &createInfo, nullptr, &framebuffer);
}

void Skybox::Draw(const CommandBuffer& cmdBuffer)
{
	
}

void Skybox::Destroy()
{
	pipelineRefCount--;
	if (pipelineRefCount <= 0)
	{
		vgm::Delete(renderPass);
		delete pipeline;
		pipeline = nullptr;
	}

	vgm::Delete(framebuffer);
	delete cubemap;
}