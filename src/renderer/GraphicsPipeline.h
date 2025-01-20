#pragma once
#include <string>
#include <vector>
#include <map>
#include <vulkan/vulkan.h>

#include "PipelineCreator.h"
#include "FramesInFlight.h"
#include "Pipeline.h"
#include "CommandBuffer.h"

namespace FIF
{
	class Buffer;
}

class ShaderGroupReflector;
class Swapchain;

class GraphicsPipeline : public Pipeline
{
public:
	struct CreateInfo
	{
		std::string vertexShader;
		std::string fragmentShader;
		VkRenderPass renderPass = VK_NULL_HANDLE;

		//maybe just flags ??
		bool noVertices  = false;
		bool noDepth     = false;
		bool noCulling   = false;
		bool noBlending  = false;
		bool cullFront   = false;
		bool frontCW     = false;
		bool polygonLine = false;
	};

	GraphicsPipeline(const CreateInfo& createInfo);
	~GraphicsPipeline();

	GraphicsPipeline(const GraphicsPipeline&) = delete;
	GraphicsPipeline& operator=(GraphicsPipeline&&) = delete;

	void Bind(CommandBuffer commandBuffer);

	void BindBufferToName(const std::string& name, VkBuffer buffer);
	void BindBufferToName(const std::string& name, const FIF::Buffer& buffer);
	void BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout);
	void BindImageToName(const std::string& name, uint32_t index, VkImageView view, VkSampler sampler, VkImageLayout layout);

	template<typename T>
	void PushConstant(CommandBuffer commandBuffer, const T& value, VkShaderStageFlags stages) 
	{
		const void* val = static_cast<const void*>(&value);
		uint32_t size = static_cast<uint32_t>(sizeof(T));

		PushConstant(commandBuffer, val, stages, size, 0); 
	}

	void PushConstant(CommandBuffer commandBuffer, const void* value, VkShaderStageFlags stages, uint32_t size, uint32_t offset = 0);

private:
	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayout(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(uint32_t amount);

	void CreatePipelineLayout(const ShaderGroupReflector& reflector);
	void CreateGraphicsPipeline(const std::vector<std::vector<char>>& shaders, const CreateInfo& createInfo, uint32_t attachmentCount);

	std::map<std::string, BindingLayout> nameToLayout;
};