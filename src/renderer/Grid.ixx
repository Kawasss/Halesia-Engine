export module Renderer.Grid;

import std;

import Renderer.GraphicsPipeline;
import Renderer.RenderPipeline;

export class GridPipeline : public RenderPipeline
{
public:
	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<MeshObject*>& objects) override;

	void ReloadShaders(const Payload& payload) override;

private:
	void CreatePipeline();

	std::unique_ptr<GraphicsPipeline> pipeline;
};