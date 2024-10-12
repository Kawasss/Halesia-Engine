#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <array>

#include "FramesInFlight.h"

class Pipeline
{
public:
	VkPipelineLayout GetLayout() const { return layout;   }
	VkPipeline GetPipeline() const     { return pipeline; }

	std::vector<VkDescriptorSet>& GetDescriptorSets() { return descriptorSets[FIF::frameIndex]; }
	std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT>& GetAllDescriptorSets() { return descriptorSets; }

protected:
	struct BindingLayout
	{
		uint32_t set;
		VkDescriptorSetLayoutBinding binding;
	};

	VkPipeline pipeline;
	VkPipelineLayout layout;

	VkDescriptorPool pool;

	std::vector<VkDescriptorSetLayout> setLayouts;
	std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT> descriptorSets;
};