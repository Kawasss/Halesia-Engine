#include "renderer/Skybox.h"
#include "renderer/Buffer.h"
#include "renderer/Renderer.h"
#include "renderer/Vulkan.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Texture.h"

#include "core/Camera.h"

void SkyboxPipeline::Start(const Payload& payload)
{
	pipeline = new GraphicsPipeline("shaders/spirv/skybox.vert.spv", "shaders/spirv/skybox.frag.spv", PIPELINE_FLAG_CULL_BACK | PIPELINE_FLAG_NO_VERTEX, renderPass);
	renderPass = PipelineCreator::CreateRenderPass(VK_FORMAT_R8G8B8A8_UNORM, RENDERPASS_FLAG_DONT_CLEAR_DEPTH, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	Cubemap* map = new Cubemap(1024, 1024);
	SetSkyBox(map);

	ubo.Init(sizeof(UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	ubo.MapPermanently();

	pipeline->BindBufferToName("ubo", ubo);	
}

void SkyboxPipeline::Execute(const Payload& payload, const std::vector<Object*>& objects)
{
	if (cubemap == nullptr)
		return;

	UBO* ptr = ubo.GetMappedPointer<UBO>();
	ptr->projection = payload.camera->GetProjectionMatrix();
	ptr->view = payload.camera->GetViewMatrix();

	payload.renderer->StartRenderPass(payload.commandBuffer, renderPass);

	pipeline->Bind(payload.commandBuffer);

	vkCmdDraw(payload.commandBuffer, 6, 1, 0, 0);

	vkCmdEndRenderPass(payload.commandBuffer);
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