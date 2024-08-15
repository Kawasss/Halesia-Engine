#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <map>

class ShaderGroupReflector;

class ComputeShader
{
public:
	ComputeShader(std::string path);
	~ComputeShader();

	void Execute(VkCommandBuffer commandBuffer, uint32_t x, uint32_t y, uint32_t z);

	void BindBufferToName(const std::string& name, VkBuffer buffer);
	void BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout);

	std::vector<VkDescriptorSet>& GetDescriptorSets() { return sets; }

private:
	struct BindingLayout
	{
		uint32_t set;
		VkDescriptorSetLayoutBinding binding;
	};

	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayout(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(uint32_t amount);
	void CreatePipelineLayout();
	void CreateComputePipeline(VkShaderModule module); // this handles the destruction for the module!

	std::map<std::string, BindingLayout> nameToLayout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::vector<VkDescriptorSet> sets;
};