#pragma once
#include <memory>

#include "RenderPipeline.h"
#include "Buffer.h"

class GraphicsPipeline;

class GridPipeline : public RenderPipeline
{
public:
	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<MeshObject*>& objects) override;
	
	void ReloadShaders(const Payload& payload) override;

private:
	struct PushConstant;

	void CreatePipeline();

	std::unique_ptr<GraphicsPipeline> pipeline;
	FIF::Buffer constants;
};