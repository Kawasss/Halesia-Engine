module;

#include "CommandBuffer.h"

export module Renderer.ComputePipeline;

import <vulkan/vulkan.h>;

import std;

import Renderer.Pipeline;

export class ComputePipeline : public Pipeline
{
public:
	ComputePipeline(const std::string_view& path);
	ComputePipeline(const ComputePipeline&) = delete;
	ComputePipeline& operator=(ComputePipeline&&) = delete;

	void Execute(const CommandBuffer& commandBuffer, std::uint32_t x, std::uint32_t y, std::uint32_t z);

private:
	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateComputePipeline(VkShaderModule module); // this handles the destruction for the module!
};