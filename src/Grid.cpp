module;

#include <Windows.h>
#include <vulkan/vulkan.h>

module Renderer.Grid;

import std;

import Renderer.GraphicsPipeline;
import Renderer.RenderPipeline;
import Renderer.RenderableMesh;
import Renderer;

void GridPipeline::Start(const Payload& payload)
{
	CreatePipeline();
}

void GridPipeline::Execute(const Payload& payload, const std::vector<RenderableMesh>& objects)
{
	const CommandBuffer& cmdBuffer = payload.commandBuffer;

	payload.presentationFramebuffer.StartRenderPass(cmdBuffer);

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
	createInfo.renderPass = renderPass3D;

	pipeline = std::make_unique<GraphicsPipeline>(createInfo);
}

void GridPipeline::ReloadShaders(const Payload& payload)
{
	CreatePipeline();
}