#pragma once
#include <memory>

#include "SimpleMesh.h"
#include "RenderPipeline.h"
#include "Buffer.h"

class GraphicsPipeline;

class BoundingVolumePipeline : public RenderPipeline
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