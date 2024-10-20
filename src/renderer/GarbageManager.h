#pragma once
#include <vulkan/vulkan.h>

namespace vgm // vulkan garbage manager
{
	extern void DeleteObject(VkObjectType type, uint64_t handle);

	template<typename T> inline void Delete(T handle)
	{
		__debugbreak();
	}

#define DECLARE_DELETOR(type, name) template<> inline void Delete<name>(name handle) { DeleteObject(type, reinterpret_cast<uint64_t>(handle)); }

	DECLARE_DELETOR(VK_OBJECT_TYPE_IMAGE, VkImage);
	DECLARE_DELETOR(VK_OBJECT_TYPE_IMAGE_VIEW, VkImageView);

	DECLARE_DELETOR(VK_OBJECT_TYPE_BUFFER, VkBuffer);
	DECLARE_DELETOR(VK_OBJECT_TYPE_DEVICE_MEMORY, VkDeviceMemory);

	DECLARE_DELETOR(VK_OBJECT_TYPE_PIPELINE, VkPipeline);
	DECLARE_DELETOR(VK_OBJECT_TYPE_PIPELINE_LAYOUT, VkPipelineLayout);

	DECLARE_DELETOR(VK_OBJECT_TYPE_DESCRIPTOR_POOL, VkDescriptorPool);
	DECLARE_DELETOR(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, VkDescriptorSetLayout);

	DECLARE_DELETOR(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, VkAccelerationStructureKHR);

#undef DECLARE_DELETOR

	extern void CollectGarbage();
	extern void ForceDelete();
}