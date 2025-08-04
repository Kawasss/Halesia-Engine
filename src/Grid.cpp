#include "renderer/Grid.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Renderer.h"

#include "core/Camera.h"

#include "glm.h"

struct GridPipeline::PushConstant
{
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 model;
	glm::vec3 camPos;
};

void GridPipeline::Start(const Payload& payload)
{
	constants.Init(sizeof(PushConstant), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	constants.MapPermanently();

	CreatePipeline();
}

void GridPipeline::Execute(const Payload& payload, const std::vector<MeshObject*>& objects)
{
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	payload.renderer->GetFramebuffer().TransitionFromReadToWrite(cmdBuffer);

	PushConstant* pConstant = constants.GetMappedPointer<PushConstant>();
	pConstant->projection = payload.camera->GetProjectionMatrix();
	pConstant->view = payload.camera->GetViewMatrix();
	pConstant->model = glm::scale(glm::vec3(1000, 1000, 1000));
	pConstant->camPos = payload.camera->position;

	payload.renderer->StartRenderPass(renderPass);

	pipeline->Bind(cmdBuffer);
	cmdBuffer.SetCullMode(VK_CULL_MODE_NONE);

	cmdBuffer.Draw(6, 1, 0, 0);

	cmdBuffer.SetCullMode(VK_CULL_MODE_BACK_BIT);
	cmdBuffer.EndRenderPass();
}

void GridPipeline::CreatePipeline()
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader = "shaders/uncompiled/grid.vert";
	createInfo.fragmentShader = "shaders/uncompiled/grid.frag";
	createInfo.noCulling = true;
	createInfo.renderPass = renderPass;

	pipeline = std::make_unique<GraphicsPipeline>(createInfo);
	pipeline->BindBufferToName("constant", constants);
}

void GridPipeline::ReloadShaders(const Payload& payload)
{
	CreatePipeline();
}