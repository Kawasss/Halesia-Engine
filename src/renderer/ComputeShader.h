#pragma once
#include <vulkan/vulkan.h>
#include <string>

#include "Pipeline.h"

class ShaderGroupReflector;
class CommandBuffer;

class ComputeShader : public Pipeline
{
public:
	ComputeShader(const std::string& path);
	ComputeShader(const ComputeShader&) = delete;
	ComputeShader& operator=(ComputeShader&&) = delete;

	void Execute(const CommandBuffer& commandBuffer, uint32_t x, uint32_t y, uint32_t z);

private:
	void CreatePipelineLayout();
	void CreateComputePipeline(VkShaderModule module); // this handles the destruction for the module!
};