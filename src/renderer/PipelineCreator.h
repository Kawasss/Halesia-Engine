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
	PIPELINE_FLAG_NO_CULLING = 1 << 7,
	PIPELINE_FLAG_POLYGON_LINE = 1 << 8,
	PIPELINE_FLAG_DONT_CLEAR_DEPTH = 1 << 9,
}; // also one with polygon mode

class PipelineCreator
{
public:
	static VkRenderPass CreateRenderPass(PhysicalDevice physicalDevice, VkFormat attachmentFormat, PipelineFlags flags, uint32_t attachmentCount, VkImageLayout initLayout, VkImageLayout finalLayout);
	static VkPipeline CreatePipeline(VkPipelineLayout layout, VkRenderPass renderPass, const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, PipelineOptions flags);
};