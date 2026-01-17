module;

#include "RenderPipeline.h"
#include "Buffer.h"

export module Renderer.BoundingVolumePipeline;

import std;

import Renderer.SimpleMesh;
import Renderer.GraphicsPipeline;

export class BoundingVolumePipeline : public RenderPipeline
{
public:
	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<MeshObject*>& objects) override;

	~BoundingVolumePipeline();

	void ReloadShaders(const Payload& payload) override;

private:
	struct UniformData;

	void CreatePipeline();
	void CreateBuffer();
	void CreateMesh();

	SimpleMesh box;

	std::unique_ptr<GraphicsPipeline> pipeline;
	FIF::Buffer constants;
};