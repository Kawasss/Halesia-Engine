#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

#include "PhysicalDevice.h"

class Swapchain;

using PipelineOptions = uint16_t;
enum PipelineFlags : PipelineOptions
{
	PIPELINE_FLAG_NONE = 0,
	PIPELINE_FLAG_NO_VERTEX = 1 << 1,
	PIPELINE_FLAG_SRGB_ATTACHMENT = 1 << 2,
	PIPELINE_FLAG_NO_BLEND = 1 << 3,
	PIPELINE_FLAG_CULL_BACK = 1 << 4,
	PIPELINE_FLAG_FRONT_CCW = 1 << 5,
	PIPELINE_FLAG_NO_CULLING = 1 << 6,
	PIPELINE_FLAG_POLYGON_LINE = 1 << 7,
	
}; // also one with polygon mode

using RenderPassOptions = uint16_t;
enum RenderPassFlags : RenderPassOptions
{
	RENDERPASS_FLAG_NONE = 0,
	RENDERPASS_FLAG_NO_DEPTH = 1 << 0,
	RENDERPASS_FLAG_CLEAR_ON_LOAD = 1 << 1,
	RENDERPASS_FLAG_DONT_CLEAR_DEPTH = 1 << 2,
};

class PipelineCreator
{
public:
	static VkRenderPass CreateRenderPass(const std::vector<VkFormat>& formats, RenderPassFlags flags, VkImageLayout initLayout, VkImageLayout finalLayout);

	static VkRenderPass CreateRenderPass(VkFormat format, RenderPassFlags flags, VkImageLayout initLayout, VkImageLayout finalLayout) { return CreateRenderPass(std::vector<VkFormat>{ format }, flags, initLayout, finalLayout); };

	static VkPipeline CreatePipeline(VkPipelineLayout layout, VkRenderPass renderPass, const std::vector<VkPipelineShaderStageCreateInfo>& shaderStages, PipelineOptions flags, uint32_t attachmentCount);
};