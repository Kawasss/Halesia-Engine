module;

#include "Buffer.h"
#include "CommandBuffer.h"

export module Renderer.Pipeline;

import std;

import Renderer.ShaderReflector;

export class Pipeline
{
public:
	Pipeline() = default;
	Pipeline(const Pipeline&) = delete;
	Pipeline& operator=(Pipeline&&) = delete;

	~Pipeline() { Destroy(); }

	VkPipelineLayout GetLayout() const { return layout; }
	VkPipeline GetPipeline() const { return pipeline; }

	std::vector<VkDescriptorSet>& GetDescriptorSets() { return descriptorSets[FIF::frameIndex]; }
	std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT>& GetAllDescriptorSets() { return descriptorSets; }

	void Bind(const CommandBuffer& commandBuffer) const;

	template<typename T>
	void PushConstant(CommandBuffer commandBuffer, const T& value, VkShaderStageFlags stages) const;
	void PushConstant(CommandBuffer commandBuffer, const void* value, VkShaderStageFlags stages, std::uint32_t size, std::uint32_t offset = 0) const;

	void BindBufferToName(const std::string& name, VkBuffer buffer);
	void BindBufferToName(const std::string& name, const FIF::Buffer& buffer);
	void BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout);
	void BindImageToName(const std::string& name, std::uint32_t index, VkImageView view, VkSampler sampler, VkImageLayout layout);

	static void AppendGlobalFIFDescriptorSets(const std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT>& sets);
	static void AppendGlobalFIFDescriptorSet(const std::array<VkDescriptorSet, FIF::FRAME_COUNT>& sets);
	static void AppendGlobalDescriptorSets(const std::span<const VkDescriptorSet>& sets);
	static void AppendGlobalDescriptorSet(VkDescriptorSet set);

	static std::vector<VkDescriptorSetLayout> globalSetLayouts;
	static std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT> globalDescriptorSets;

protected:
	void InitializeBase(const ShaderGroupReflector& reflector);

	struct BindingLayout
	{
		std::uint32_t set;
		VkDescriptorSetLayoutBinding binding;
	};

	VkPipeline pipeline;
	VkPipelineLayout layout;

	VkDescriptorPool pool;

	VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;

	std::vector<VkDescriptorSetLayout> setLayouts;
	std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT> descriptorSets;

	std::map<std::string, BindingLayout> nameToLayout;

private:
	void Destroy();

	void CreateDescriptorPool(const ShaderGroupReflector& reflector);
	void CreateSetLayouts(const ShaderGroupReflector& reflector);
	void AllocateDescriptorSets(std::uint32_t amount);

	void InsertGlobalLayouts();
	void InsertGlobalSets();
};

template<typename T>
void Pipeline::PushConstant(CommandBuffer commandBuffer, const T& value, VkShaderStageFlags stages) const
{
	const void* val = static_cast<const void*>(&value);
	std::uint32_t size = static_cast<std::uint32_t>(sizeof(T));

	PushConstant(commandBuffer, val, stages, size, 0);
}