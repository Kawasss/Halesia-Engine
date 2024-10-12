#include "renderer/Skybox.h"
#include "renderer/Buffer.h"
#include "renderer/Renderer.h"
#include "renderer/Vulkan.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Texture.h"

#include "core/Camera.h"

void SkyboxPipeline::Start(const Payload& payload)
{
	CreateRenderPass();
	CreatePipeline();

	Cubemap* map = new Cubemap(1024, 1024);
	SetSkyBox(map);

	ubo.Init(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	ubo.MapPermanently();

	pipeline->BindBufferToName("ubo", ubo);	
}

void SkyboxPipeline::CreateRenderPass()
{
	RenderPassBuilder builder(VK_FORMAT_R8G8B8A8_UNORM);

	builder.SetInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
	builder.SetFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	renderPass = builder.Build();
}

void SkyboxPipeline::CreatePipeline()
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader   = "shaders/spirv/skybox.vert.spv";
	createInfo.fragmentShader = "shaders/spirv/skybox.frag.spv";

	createInfo.renderPass = renderPass;
	createInfo.noVertices = true;

	pipeline = new GraphicsPipeline(createInfo);
}

void SkyboxPipeline::Execute(const Payload& payload, const std::vector<Object*>& objects)
{
	if (cubemap == nullptr)
		return;

	const CommandBuffer cmdBuffer = payload.commandBuffer;

	UBO* ptr = ubo.GetMappedPointer<UBO>();
	ptr->projection = payload.camera->GetProjectionMatrix();
	ptr->view = payload.camera->GetViewMatrix();

	payload.renderer->StartRenderPass(payload.commandBuffer, renderPass);

	pipeline->Bind(payload.commandBuffer);

	cmdBuffer.Draw(6, 1, 0, 0);

	cmdBuffer.EndRenderPass();
}

void SkyboxPipeline::Destroy()
{
	if (cubemap) delete cubemap;
	delete pipeline;

	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkDestroyRenderPass(ctx.logicalDevice, renderPass, nullptr);

	ubo.Destroy();
}

void SkyboxPipeline::SetSkyBox(Cubemap* skybox)
{
	if (cubemap) delete cubemap;
	cubemap = skybox;

	pipeline->BindImageToName("skybox", skybox->imageView, Renderer::defaultSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}