#pragma once
#include <vulkan/vulkan.h>

class DescriptorWriter // the writer basically does buffered writing, the size of the buffer is determined by MAX_INFO_COUNT.
{
public:
	static DescriptorWriter* Get();

	void Write();

	void WriteBuffer(VkDescriptorSet set, VkBuffer buffer, VkDescriptorType type, uint32_t binding, uint32_t descriptorCount = 1, VkDeviceSize range = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	void WriteImage(VkDescriptorSet set, VkDescriptorType type, uint32_t binding, VkImageView imageView, VkSampler sampler, VkImageLayout layout, uint32_t descriptorCount = 1, uint32_t index = 0);

private:
	union GenericDescriptorInfo // this union stores the different types of descriptor writes to reduce memory footprint
	{
		VkDescriptorBufferInfo buffer;
		VkDescriptorImageInfo  image;
	};

	static constexpr size_t MAX_INFO_COUNT = 64;
	
	GenericDescriptorInfo infos[MAX_INFO_COUNT]{};
	VkWriteDescriptorSet  writeSets[MAX_INFO_COUNT]{};
	int infoSize = 0;
};