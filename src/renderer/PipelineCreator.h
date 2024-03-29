#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

class PhysicalDevice;
class Swapchain;

typedef uint16_t PipelineOptions;
enum PipelineFlags : PipelineOptions
{
	PIPELINE_FLAG_NONE = 0,
	PIPELINE_FLAG_NO_DEPTH = 1 << 0,
	PIPELINE_FLAG_NO_VERTEX = 1 << 1,
	PIPELINE_FLAG_CLEAR_ON_LOAD = 1 << 2,
	PIPELINE_FLAG_SRGB_ATTACHMENT = 1 << 3,
	PIPELINE_FLAG_NO_BLEND = 1 << 4,
	PIPELINE_FLAG_CULL_BACK = 1 << 5,
	PIPELINE_FLAG_FRONT_CCW = 1 << 6,
}; // also one with polygon mode

class PipelineCreator
{
public:
	static VkRenderPass CreateRenderPass(PhysicalDevice physicalDevice, Swapchain* swapchain, PipelineFlags flags, uint32_t attachmentCount);
	static VkPipeline CreatePipeline(VkPipelineLayout layout, VkRenderPass renderPass, Swapchain* swapchain, const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, PipelineOptions flags);
};