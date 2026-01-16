#include <cassert>

#include "renderer/Pipeline.h"
#include "renderer/ShaderReflector.h"
#include "renderer/CommandBuffer.h"
#include "renderer/GarbageManager.h"
#include "renderer/VulkanAPIError.h"
#include "renderer/Buffer.h"
#include "renderer/Vulkan.h"

#include "core/Console.h"

import Renderer.DescriptorWriter;

std::vector<VkDescriptorSetLayout> Pipeline::globalSetLayouts;
std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT> Pipeline::globalDescriptorSets;

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

	if (poolSizes.empty())
		return;

	for (VkDescriptorPoolSize& size : poolSizes)
		size.descriptorCount *= FIF::FRAME_COUNT;

	VkDescriptorPoolCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
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

	if (indices.empty())
	{
		InsertGlobalLayouts();
		return;
	}

	setLayouts.reserve(indices.size() + globalSetLayouts.size());

	for (uint32_t index : indices)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings = reflector.GetLayoutBindingsOfSet(index);
		std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

		VkDescriptorSetLayoutBindingFlagsCreateInfo flagCreateInfo{};
		flagCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
		flagCreateInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
		flagCreateInfo.pBindingFlags = bindingFlags.data();

		VkDescriptorSetLayoutCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		createInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		createInfo.pBindings = bindings.data();
		createInfo.pNext = &flagCreateInfo;

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

	InsertGlobalLayouts();
}

void Pipeline::AllocateDescriptorSets(uint32_t amount)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	if (pool != VK_NULL_HANDLE)
	{
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
	}

	InsertGlobalSets();
}

void Pipeline::InsertGlobalLayouts()
{
	if (!globalSetLayouts.empty())
		setLayouts.insert(setLayouts.end(), globalSetLayouts.begin(), globalSetLayouts.end());
}

void Pipeline::InsertGlobalSets()
{
	if (globalDescriptorSets[0].empty())
		return;

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
	{
		if (globalDescriptorSets[i].empty())
			continue;

		std::vector<VkDescriptorSet>& sets = descriptorSets[i];
		const std::vector<VkDescriptorSet>& globalSets = globalDescriptorSets[i];

		sets.insert(sets.end(), globalSets.begin(), globalSets.end());
	}
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
	if (!nameToLayout.contains(name))
	{
		Console::WriteLine("attempted to bind buffer to non-existing name \"{}\"", Console::Severity::Warning, name);
		return;
	}

	const BindingLayout& binding = nameToLayout.at(name);

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		DescriptorWriter::WriteBuffer(descriptorSets[i][binding.set], buffer, binding.binding.descriptorType, binding.binding.binding);
}

void Pipeline::BindBufferToName(const std::string& name, const FIF::Buffer& buffer)
{
	if (!nameToLayout.contains(name))
	{
		Console::WriteLine("attempted to bind FIF buffer to non-existing name \"{}\"", Console::Severity::Warning, name);
		return;
	}

	const BindingLayout& binding = nameToLayout.at(name);

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		DescriptorWriter::WriteBuffer(descriptorSets[i][binding.set], buffer[i], binding.binding.descriptorType, binding.binding.binding);
}

void Pipeline::BindImageToName(const std::string& name, VkImageView view, VkSampler sampler, VkImageLayout layout)
{
	if (!nameToLayout.contains(name))
	{
		Console::WriteLine("attempted to bind image to non-existing name \"{}\"", Console::Severity::Warning, name);
		return;
	}

	const BindingLayout& binding = nameToLayout.at(name);

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		DescriptorWriter::WriteImage(descriptorSets[i][binding.set], binding.binding.descriptorType, binding.binding.binding, view, sampler, layout);
}

void Pipeline::BindImageToName(const std::string& name, uint32_t index, VkImageView view, VkSampler sampler, VkImageLayout layout)
{
	if (!nameToLayout.contains(name))
	{
		Console::WriteLine("attempted to bind image to non-existing name \"{}\"", Console::Severity::Warning, name);
		return;
	}

	const BindingLayout& binding = nameToLayout.at(name);

	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		DescriptorWriter::WriteImage(descriptorSets[i][binding.set], binding.binding.descriptorType, binding.binding.binding, view, sampler, layout, 1, index);
}

void Pipeline::AppendGlobalFIFDescriptorSets(const std::array<std::vector<VkDescriptorSet>, FIF::FRAME_COUNT>& sets)
{
	for (int i = 0; i < FIF::FRAME_COUNT; i++)
	{
		const std::vector<VkDescriptorSet>& src = sets[i];
		std::vector<VkDescriptorSet>& dst = globalDescriptorSets[i];

		dst.insert(dst.end(), src.begin(), src.end());
	}
}

void Pipeline::AppendGlobalFIFDescriptorSet(const std::array<VkDescriptorSet, FIF::FRAME_COUNT>& sets)
{
	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		globalDescriptorSets[i].push_back(sets[i]);
}

void Pipeline::AppendGlobalDescriptorSets(const std::span<const VkDescriptorSet>& sets)
{
	for (int i = 0; i < FIF::FRAME_COUNT; i++)
	{
		std::vector<VkDescriptorSet>& dst = globalDescriptorSets[i];
		dst.insert(dst.end(), sets.begin(), sets.end());
	}
}

void Pipeline::AppendGlobalDescriptorSet(VkDescriptorSet set)
{
	for (int i = 0; i < FIF::FRAME_COUNT; i++)
		globalDescriptorSets[i].push_back(set);
}