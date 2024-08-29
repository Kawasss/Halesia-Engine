#pragma once
#include "RenderPipeline.h"
#include "Framebuffer.h"
#include "Buffer.h"

class GraphicsPipeline;

class DeferredPipeline : public RenderPipeline
{
public:
	void Start(const Payload& payload) override;
	void Execute(const Payload& payload, const std::vector<Object*>& objects) override;
	void Destroy() override;

private:
	Framebuffer framebuffer;

	GraphicsPipeline* firstPipeline  = nullptr;
	GraphicsPipeline* secondPipeline = nullptr;
};