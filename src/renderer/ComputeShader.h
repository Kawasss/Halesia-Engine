#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class ShaderGroupReflector;

class ComputeShader
{
public:
	ComputeShader(std::string path);
	~ComputeShader();

	void Execute(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z);
	void WriteToDescriptorBuffer(VkBuffer buffer, VkDescriptorType type, uint32_t setIndex, uint32_t binding, VkDeviceSize range = VK_WHOLE_SIZE);

private:
	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayout(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(uint32_t amount);
	void CreatePipelineLayout();
	void CreateComputePipeline(VkShaderModule module); // this handles the destruction for the module!

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> sets;
};