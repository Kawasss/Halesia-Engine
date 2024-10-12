#pragma once
#include <string>
#include <vector>
#include <map>
#include <array>
#include <vulkan/vulkan.h>

#include "PipelineCreator.h"
#include "FramesInFlight.h"
#include "Pipeline.h"

namespace FIF
{
	class Buffer;
}

class ShaderGroupReflector;
class Swapchain;

class GraphicsPipeline : public Pipeline
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

	template<typename T>
	void PushConstant(VkCommandBuffer commandBuffer, const T& value, VkShaderStageFlags stages) 
	{
		const void* val = static_cast<const void*>(&value);
		uint32_t size = static_cast<uint32_t>(sizeof(T));

		PushConstant(commandBuffer, val, stages, size, 0); 
	}

	void PushConstant(VkCommandBuffer commandBuffer, const void* value, VkShaderStageFlags stages, uint32_t size, uint32_t offset = 0);

private:
	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayout(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(uint32_t amount);

	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateGraphicsPipeline(const std::vector<std::vector<char>>& shaders, PipelineOptions flags, VkRenderPass renderPass, uint32_t attachmentCount);

	std::map<std::string, BindingLayout> nameToLayout;
};