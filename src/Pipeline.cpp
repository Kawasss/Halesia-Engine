#include <cassert>

#include "renderer/Pipeline.h"
#include "renderer/ShaderReflector.h"
#include "renderer/CommandBuffer.h"
#include "renderer/DescriptorWriter.h"
#include "renderer/GarbageManager.h"
#include "renderer/VulkanAPIError.h"
#include "renderer/Buffer.h"
#include "renderer/Vulkan.h"

std::vector<VkDescriptorSetLayout> Pipeline::globalSetLayouts;
std::vector<VkDescriptorSet> Pipeline::globalDescriptorSets;

void Pipeline::InitializeBase(const ShaderGroupReflector& reflector)
{
	CreateDescriptorPool(reflector);
	CreateSetLayouts(reflector);

	AllocateDescriptorSets(reflector.GetDescriptorSetCount());
}

void Pipeline::CreateDescriptorPool(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	std::vector<VkDescriptorPoolSize> poolSizes = reflector.GetDescriptorPoolSize();

	for (VkDescriptorPoolSize& size : poolSizes)
		size.descriptorCount *= FIF::FRAME_COUNT;

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.maxSets = reflector.GetDescriptorSetCount() * FIF::FRAME_COUNT;
	createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	createInfo.pPoolSizes = poolSizes.data();

	VkResult result = vkCreateDescriptorPool(ctx.logicalDevice, &createInfo, nullptr, &pool);
	CheckVulkanResult("Failed to create the descriptor pool for a graphics pipeline", result);
}

void Pipeline::CreateSetLayouts(const ShaderGroupReflector& reflector)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	std::set<uint32_t> indices = reflector.GetDescriptorSetIndices();

	setLayouts.reserve(indices.size());

	for (uint32_t index : indices)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = reflector.GetLayoutBindingsOfSet(index);

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		createInfo.pBindings = bindings.data();

		VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
		VkResult result = vkCreateDescriptorSetLayout(ctx.logicalDevice, &createInfo, nullptr, &setLayout);
		CheckVulkanResult("Failed to create a set layout for a graphics pipeline", result);

		setLayouts.push_back(setLayout);

		for (const VkDescriptorSetLayoutBinding& binding : bindings)
		{
			ShaderGroupReflector::Binding bind(index, binding.binding);
			BindingLayout& bindingLayout = nameToLayout[reflector.GetNameOfBinding(bind)];
			bindingLayout.set = index;
			bindingLayout.binding = binding;
		}
	}

	if (!globalSetLayouts.empty())
		setLayouts.insert(setLayouts.end(), globalSetLayouts.begin(), globalSetLayouts.end());
}

void Pipeline::AllocateDescriptorSets(uint32_t amount)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = amount;
	allocInfo.descriptorPool = pool;
	allocInfo.pSetLayouts = setLayouts.data();

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
	{
		descriptorSets[i].resize(amount);
		VkResult result = vkAllocateDescriptorSets(ctx.logicalDevice, &allocInfo, descriptorSets[i].data());
		CheckVulkanResult("Failed to allocate descriptor sets for a graphics pipeline", result);
	}

	if (globalDescriptorSets.empty())
		return;

	for (std::vector<VkDescriptorSet>& sets : descriptorSets)
		sets.insert(sets.end(), globalDescriptorSets.begin(), globalDescriptorSets.end());
}

void Pipeline::Destroy()
{
	vgm::Delete(pipeline);
	vgm::Delete(layout);

	for (const VkDescriptorSetLayout& setLayout : setLayouts)
		if (std::find(globalSetLayouts.begin(), globalSetLayouts.end(), setLayout) == globalSetLayouts.end())
			vgm::Delete(setLayout);

	vgm::Delete(pool);
}

void Pipeline::Bind(const CommandBuffer& cmdBuffer) const
{
	assert(bindPoint != VK_PIPELINE_BIND_POINT_MAX_ENUM && "Invalid bind point set");

	uint32_t setCount = static_cast<uint32_t>(descriptorSets[FIF::frameIndex].size());

	cmdBuffer.BindPipeline(bindPoint, pipeline);
	cmdBuffer.BindDescriptorSets(bindPoint, layout, 0, setCount, descriptorSets[FIF::frameIndex].data(), 0, nullptr);
}

void Pipeline::PushConstant(CommandBuffer commandBuffer, const void* value, VkShaderStageFlags stages, uint32_t size, uint32_t offset) const
{
	commandBuffer.PushConstants(layout, stages, offset, size, value);
}

void Pipeline::BindBufferToName(const std::string& name, VkBuffer buffer)
{
	const BindingLayout& binding = nameToLayout[name];

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		DescriptorWriter::WriteBuffer(descriptorSets[i][binding.set], buffer, binding.binding.descriptorType, binding.binding.binding);
}

void Pipeline::BindBufferToName(const std::string& name, const FIF::Buffer& buffer)
{
	const BindingLayout& binding = nameToLayout[name];

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		DescriptorWriter::WriteBuffer(descriptorSets[i][binding.set], buffer[i], binding.binding.descriptorType, binding.binding.binding);
}

void Pipeline::BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout)
{
	const BindingLayout& binding = nameToLayout[name];

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		DescriptorWriter::WriteImage(descriptorSets[i][binding.set], binding.binding.descriptorType, binding.binding.binding, view, sampler, layout);
}

void Pipeline::BindImageToName(const std::string& name, uint32_t index, VkImageView view, VkSampler sampler, VkImageLayout layout)
{
	const BindingLayout& binding = nameToLayout[name];

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		DescriptorWriter::WriteImage(descriptorSets[i][binding.set], binding.binding.descriptorType, binding.binding.binding, view, sampler, layout, 1, index);
}