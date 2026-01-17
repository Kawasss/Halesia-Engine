module;

#include <vulkan/vulkan.h>

#include "Pipeline.h"

export module Renderer.ComputePipeline;

import std;

export class ComputeShader : public Pipeline
{
public:
	ComputeShader(const std::string_view& path);
	ComputeShader(const ComputeShader&) = delete;
	ComputeShader& operator=(ComputeShader&&) = delete;

	void Execute(const CommandBuffer& commandBuffer, std::uint32_t x, std::uint32_t y, std::uint32_t z);

private:
	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateComputePipeline(VkShaderModule module); // this handles the destruction for the module!
};