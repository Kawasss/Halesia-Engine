#include "renderer/Skybox.h"
#include "renderer/Buffer.h"
#include "renderer/Renderer.h"
#include "renderer/Vulkan.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Texture.h"
#include "renderer/DescriptorWriter.h"

#include "core/Camera.h"

struct UBO
{
	glm::mat4 projection;
	glm::mat4 view;
};

void SkyboxPipeline::Start(const Payload& payload)
{
	CreateRenderPass();
	CreatePipeline();

	texture = new Texture("textures/skybox/park.hdr", false);
	texture->AwaitGeneration();

	Vulkan::SetDebugName(texture->image, "skybox 2d");

	convertPipeline->BindImageToName("equirectangularMap", texture->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	DescriptorWriter::Get()->Write();

	Cubemap* map = new Cubemap(1024, 1024);

	SetSkyBox(map);

	SetupConvert(payload);

	ubo.Init(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	ubo.MapPermanently();

	pipeline->BindBufferToName("ubo", ubo);	
}

void SkyboxPipeline::CreateRenderPass()
{
	RenderPassBuilder builder(VK_FORMAT_R8G8B8A8_UNORM);

	builder.SetInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	builder.SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	builder.ClearOnLoad(false);
	builder.DontClearDepth(true);

	renderPass = builder.Build();

	RenderPassBuilder convertBuilder(VK_FORMAT_R8G8B8A8_SRGB);

	convertBuilder.SetInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	convertBuilder.SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	convertBuilder.DisableDepth(true);
	convertBuilder.ClearOnLoad(true);

	convertRenderPass = convertBuilder.Build();
}

void SkyboxPipeline::CreatePipeline()
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/spirv/skybox.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/skybox.frag.spv";

	createInfo.renderPass = renderPass;
	createInfo.noVertices = true;
	createInfo.noCulling  = true;

	pipeline = new GraphicsPipeline(createInfo);

	createInfo.vertexShader   = "shaders/spirv/cubemapConv.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/cubemapConv.frag.spv";

	createInfo.renderPass = convertRenderPass;
	createInfo.noVertices = true;
	createInfo.noCulling  = true;
	createInfo.noDepth    = true;

	convertPipeline = new GraphicsPipeline(createInfo);
}

void SkyboxPipeline::SetupConvert(const Payload& payload)
{
	const VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

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
	createInfo.renderPass = convertRenderPass;

	vkCreateFramebuffer(Vulkan::GetContext().logicalDevice, &createInfo, nullptr, &framebuffer);
}

void SkyboxPipeline::ConvertImageToCubemap(const Payload& payload)
{
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	Renderer::SetViewport(cmdBuffer, { 1024, 1024 });
	Renderer::SetScissors(cmdBuffer, { 1024, 1024 });

	const std::array<glm::mat4, 6> views =
	{
		glm::lookAt(glm::vec3(0), glm::vec3( 1,  0,  0), glm::vec3(0, -1,  0)),
		glm::lookAt(glm::vec3(0), glm::vec3(-1,  0,  0), glm::vec3(0, -1,  0)),
		glm::lookAt(glm::vec3(0), glm::vec3( 0,  1,  0), glm::vec3(0,  0,  1)),
		glm::lookAt(glm::vec3(0), glm::vec3( 0, -1,  0), glm::vec3(0,  0, -1)),
		glm::lookAt(glm::vec3(0), glm::vec3( 0,  0,  1), glm::vec3(0, -1,  0)),
		glm::lookAt(glm::vec3(0), glm::vec3( 0,  0, -1), glm::vec3(0, -1,  0)),
	};
	UBO pushConstant{};
	pushConstant.projection = glm::perspective(glm::pi<float>() * 0.5f, 1.0f, 0.01f, 1000.0f);
	pushConstant.projection[1][1] *= -1;

	Renderer::BindBuffersForRendering(cmdBuffer);

	convertPipeline->Bind(cmdBuffer);

	for (int i = 0; i < 6; i++)
	{
		int actual = i;
		if (i == 3) // reverse the up and down texture
			actual = 2;
		else if (i == 2)
			actual = 3;

		pushConstant.view = views[actual];

		convertPipeline->PushConstant(cmdBuffer, pushConstant, VK_SHADER_STAGE_VERTEX_BIT);

		std::array<VkClearValue, 1> clearColors{};
		clearColors[0].color = { 1, 0, 0, 1 };

		VkRenderPassAttachmentBeginInfo attachInfo{};
		attachInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_ATTACHMENT_BEGIN_INFO;
		attachInfo.attachmentCount = 1;
		attachInfo.pAttachments = &cubemap->layerViews[i];

		::VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = &attachInfo;
		renderPassBeginInfo.renderPass = convertRenderPass;
		renderPassBeginInfo.framebuffer = framebuffer;
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = { 1024, 1024 };
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearColors.size());
		renderPassBeginInfo.pClearValues = clearColors.data();

		cmdBuffer.BeginRenderPass(renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		cmdBuffer.Draw(36, 1, 0, 0);
		cmdBuffer.EndRenderPass();
	}

	hasConverted = true;
}

void SkyboxPipeline::Execute(const Payload& payload, const std::vector<Object*>& objects)
{
	if (cubemap == nullptr)
		return;

	if (!hasConverted)
	{
		ConvertImageToCubemap(payload);
		return;
	}
		
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	UBO* ptr = ubo.GetMappedPointer<UBO>();
	ptr->projection = payload.camera->GetProjectionMatrix();
	ptr->view = payload.camera->GetViewMatrix();

	payload.renderer->StartRenderPass(renderPass);

	pipeline->Bind(payload.commandBuffer);

	cmdBuffer.Draw(36, 1, 0, 0);

	cmdBuffer.EndRenderPass();
}

void SkyboxPipeline::Destroy()
{
	if (cubemap) delete cubemap;

	delete pipeline;
	delete convertPipeline;

	delete texture;

	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkDestroyRenderPass(ctx.logicalDevice, renderPass, nullptr);
	vkDestroyRenderPass(ctx.logicalDevice, convertRenderPass, nullptr);

	vkDestroyFramebuffer(ctx.logicalDevice, framebuffer, nullptr);

	ubo.Destroy();
}

void SkyboxPipeline::SetSkyBox(Cubemap* skybox)
{
	if (cubemap) delete cubemap;
	cubemap = skybox;

	pipeline->BindImageToName("skybox", skybox->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}