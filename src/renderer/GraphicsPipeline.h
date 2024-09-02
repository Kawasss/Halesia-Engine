#pragma once
#include <string>
#include <vector>
#include <map>
#include <array>
#include <vulkan/vulkan.h>

#include "PipelineCreator.h"
#include "FramesInFlight.h"

namespace FIF
{
	class Buffer;
}

class ShaderGroupReflector;
class Swapchain;

class GraphicsPipeline
{
public:
	GraphicsPipeline(const std::string& vertPath, const std::string& fragPath, PipelineOptions flags, VkRenderPass renderPass);
	~GraphicsPipeline();

	GraphicsPipeline(const GraphicsPipeline&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

	void Bind(VkCommandBuffer commandBuffer);

	void BindBufferToName(const std::string& name, VkBuffer buffer);
	void BindBufferToName(const std::string& name, const FIF::Buffer& buffer);
	void BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout);
	void BindImageToName(const std::string& name, uint32_t index, VkImageView view, VkSampler sampler, VkImageLayout layout);

	std::vector<VkDescriptorSet>& GetDescriptorSets() { return descriptorSets[FIF::frameIndex]; }
	
	std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT>& GetAllDescriptorSets() { return descriptorSets; }

	VkPipelineLayout GetLayout() { return layout; }

private:
	struct BindingLayout
	{
		uint32_t set;
		VkDescriptorSetLayoutBinding binding;
	};

	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayout(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(uint32_t amount);

	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateGraphicsPipeline(const std::vector<std::vector<char>>& shaders, PipelineOptions flags, VkRenderPass renderPass, uint32_t attachmentCount);

	std::map<std::string, BindingLayout> nameToLayout;

	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;

	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSetLayout> setLayouts;
	std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT> descriptorSets;
};