#include <array>

#include "renderer/BoundingVolumePipeline.h"
#include "renderer/GraphicsPipeline.h"
#include "renderer/Renderer.h"

#include "core/MeshObject.h"
#include "core/Camera.h"

#include "glm.h"

constexpr uint32_t BOX_LINE_COUNT = 24;

struct BoundingVolumePipeline::UniformData
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 model;
};

void BoundingVolumePipeline::Start(const Payload& payload)
{
	CreateBuffer();
	CreatePipeline();
	CreateMesh();
}

void BoundingVolumePipeline::Execute(const Payload& payload, const std::vector<MeshObject*>& objects) // !!!! itd be a lot better to do instanced rendering here
{
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	UniformData* pData = constants.GetMappedPointer<UniformData>();
	pData->view = payload.camera->GetViewMatrix();
	pData->proj = payload.camera->GetProjectionMatrix();

	payload.renderer->StartRenderPass(renderPass);

	pipeline->Bind(cmdBuffer);
	Renderer::BindBuffersForRendering(cmdBuffer);

	uint32_t offset = static_cast<uint32_t>(Renderer::g_vertexBuffer.GetItemOffset(box.vertexMemory));

	for (MeshObject* pMesh : objects)
	{
		glm::mat4 model = pMesh->transform.GetModelMatrix();

		glm::vec3 pos, scale, skew;
		glm::vec4 perspective;
		glm::quat rot;

		glm::decompose(model, scale, rot, pos, skew, perspective);

		pMesh->mesh.UpdateMinMax(model);

		glm::vec3 center = (pMesh->mesh.max + pMesh->mesh.min) * 0.5f;
		glm::vec3 extents = pMesh->mesh.max - center;

		pipeline->PushConstant(cmdBuffer, glm::translate(pos) * glm::scale(extents), VK_SHADER_STAGE_VERTEX_BIT);
		cmdBuffer.Draw(BOX_LINE_COUNT, 1, offset, 0);
	}
	
	cmdBuffer.EndRenderPass();
}

BoundingVolumePipeline::~BoundingVolumePipeline()
{
	box.Destroy();
}

void BoundingVolumePipeline::ReloadShaders(const Payload& payload)
{
	CreatePipeline();
}

void BoundingVolumePipeline::CreatePipeline()
{
	GraphicsPipeline::CreateInfo createInfo{};
	createInfo.vertexShader = "shaders/uncompiled/boundingVolume.vert";
	createInfo.fragmentShader = "shaders/uncompiled/boundingVolume.frag";
	createInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	createInfo.noCulling = true;
	createInfo.renderPass = renderPass;

	pipeline = std::make_unique<GraphicsPipeline>(createInfo);
	pipeline->BindBufferToName("constants", constants);
}

void BoundingVolumePipeline::CreateBuffer()
{
	constants.Init(sizeof(UniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	constants.MapPermanently();
}

void BoundingVolumePipeline::CreateMesh()
{
	std::array<Vertex, BOX_LINE_COUNT> vertices =
	{
		glm::vec3(-1,  1, -1), glm::vec3(-1, -1, -1),
		glm::vec3(-1,  1, -1), glm::vec3(-1,  1,  1),
		glm::vec3(-1,  1, -1), glm::vec3( 1,  1, -1),
		glm::vec3(-1, -1, -1), glm::vec3( 1, -1, -1),
		glm::vec3(-1, -1, -1), glm::vec3(-1, -1,  1),
		glm::vec3(-1,  1,  1), glm::vec3( 1,  1,  1),
		glm::vec3(-1,  1,  1), glm::vec3(-1, -1,  1),
		glm::vec3( 1,  1, -1), glm::vec3( 1,  1,  1),
		glm::vec3( 1,  1, -1), glm::vec3( 1, -1, -1),
		glm::vec3( 1, -1,  1), glm::vec3( 1,  1,  1),
		glm::vec3( 1, -1,  1), glm::vec3(-1, -1,  1),
		glm::vec3( 1, -1,  1), glm::vec3( 1, -1, -1),
	};

	box = SimpleMesh::Create(vertices);
}