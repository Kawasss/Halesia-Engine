#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

class Surface;

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
	std::optional<uint32_t> computeFamily;
	std::optional<uint32_t> transferFamily;

	bool HasValue()
	{
		return graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value() && transferFamily.has_value();
	}
};

class PhysicalDevice
{
	public:
		PhysicalDevice() = default;
		PhysicalDevice(VkPhysicalDevice physicalDevice);

		uint64_t VRAM()          const;
		uint64_t AdditionalRAM() const;
		
		QueueFamilyIndices QueueFamilies(Surface& surface) const;
		VkFormat GetDepthFormat() const;

		VkPhysicalDeviceProperties       Properties()       const;
		VkPhysicalDeviceMemoryProperties MemoryProperties() const;
		VkPhysicalDeviceFeatures         Features()         const;

		VkDevice GetLogicalDevice(Surface& surface);
		VkPhysicalDevice Device() const;

		bool IsValid() { return physicalDevice != VK_NULL_HANDLE; }

	private:
		VkFormat GetSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
};