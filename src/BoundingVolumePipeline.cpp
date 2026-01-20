module;

#include <Windows.h>

#include "glm.h"

module Renderer.BoundingVolumePipeline;

import std;

import Core.CameraObject;
import Core.MeshObject;

import Renderer.SimpleMesh;
import Renderer.GraphicsPipeline;
import Renderer.RenderPipeline;
import Renderer;

constexpr uint32_t BOX_LINE_COUNT = 24;

struct BoundingVolumePipeline::UniformData
{
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 models[Renderer::MAX_MESHES];
};

void BoundingVolumePipeline::Start(const Payload& payload)
{
	CreateBuffer();
	CreatePipeline();
	CreateMesh();
}

void BoundingVolumePipeline::Execute(const Payload& payload, const std::vector<MeshObject*>& objects)
{
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	UniformData* pData = constants.GetMappedPointer<UniformData>();

	payload.presentationFramebuffer.StartRenderPass(cmdBuffer);

	pipeline->Bind(cmdBuffer);
	Renderer::BindBuffersForRendering(cmdBuffer);

	uint32_t offset = static_cast<uint32_t>(Renderer::g_vertexBuffer.GetItemOffset(box.vertexMemory));

	for (int i = 0; i < objects.size(); i++)
	{
		MeshObject* pMesh = objects[i];

		pMesh->mesh.UpdateMinMax(pMesh->transform.position, pMesh->transform.scale);

		glm::vec3 center = (pMesh->mesh.max + pMesh->mesh.min) * 0.5f;
		glm::vec3 extents = pMesh->mesh.max - center;

		pData->models[i] = glm::translate(pMesh->transform.position) * glm::scale(extents);
	}
	
	cmdBuffer.Draw(BOX_LINE_COUNT, static_cast<uint32_t>(objects.size()), offset, 0);

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
	createInfo.renderPass = renderPass3D;

	pipeline = std::make_unique<GraphicsPipeline>(createInfo);
	pipeline->BindBufferToName("constants", constants);
}

void BoundingVolumePipeline::CreateBuffer()
{
	constants.Init(sizeof(UniformData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
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