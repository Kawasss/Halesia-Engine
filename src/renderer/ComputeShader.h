#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <map>

#include "Pipeline.h"
#include "CommandBuffer.h"

class ShaderGroupReflector;

class ComputeShader : public Pipeline
{
public:
	ComputeShader(std::string path);
	~ComputeShader();

	void Execute(CommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z);

	void BindBufferToName(const std::string& name, VkBuffer buffer);
	void BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout);

private:
	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayout(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(uint32_t amount);
	void CreatePipelineLayout();
	void CreateComputePipeline(VkShaderModule module); // this handles the destruction for the module!

	std::map<std::string, BindingLayout> nameToLayout;
};