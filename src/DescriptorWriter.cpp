#include "renderer/DescriptorWriter.h"
#include "renderer/Vulkan.h"

DescriptorWriter* DescriptorWriter::Get()
{
	static DescriptorWriter* ptr = nullptr;
	if (ptr == nullptr)
		ptr = new DescriptorWriter;

	return ptr;
}
#include <iostream>
void DescriptorWriter::Write()
{
	if (infoSize == 0)
		return;

	const Vulkan::Context& ctx = Vulkan::GetContext();
	vkUpdateDescriptorSets(ctx.logicalDevice, infoSize, writeSets, 0, nullptr);

	infoSize = 0;
}

void DescriptorWriter::WriteBuffer(VkDescriptorSet set, VkBuffer buffer, VkDescriptorType type, uint32_t binding, uint32_t descriptorCount, VkDeviceSize range, VkDeviceSize offset)
{
	VkDescriptorBufferInfo& info = infos[infoSize].buffer;
	info.buffer = buffer;
	info.offset = offset;
	info.range  = range;

	VkWriteDescriptorSet& writeSet = writeSets[infoSize];
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = type;
	writeSet.dstSet = set;
	writeSet.dstBinding = binding;
	writeSet.descriptorCount = descriptorCount;
	writeSet.pBufferInfo = &info;

	infoSize++;
	if (infoSize >= MAX_INFO_COUNT)
		Write();
}

void DescriptorWriter::WriteImage(VkDescriptorSet set, VkDescriptorType type, uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, uint32_t descriptorCount)
{
	VkDescriptorImageInfo& info = infos[infoSize].image;
	info.imageLayout = layout;
	info.imageView   = imageView;
	info.sampler     = sampler;

	VkWriteDescriptorSet& writeSet = writeSets[infoSize];
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = type;
	writeSet.dstSet = set;
	writeSet.dstBinding = binding;
	writeSet.descriptorCount = descriptorCount;
	writeSet.pImageInfo = &info;

	infoSize++;
	if (infoSize >= MAX_INFO_COUNT)
		Write();
}