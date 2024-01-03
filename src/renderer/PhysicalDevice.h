#pragma once
#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

class Surface;

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	bool HasValue()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class PhysicalDevice
{
	public:
		PhysicalDevice() = default;
		PhysicalDevice(VkPhysicalDevice physicalDevice);
		uint64_t VRAM();
		uint64_t AdditionalRAM();
		VkDevice GetLogicalDevice(Surface& surface);
		QueueFamilyIndices QueueFamilies(Surface& surface);
		VkFormat GetDepthFormat();
		VkPhysicalDeviceProperties Properties();
		VkPhysicalDeviceMemoryProperties MemoryProperties();
		VkPhysicalDeviceFeatures Features();
		VkPhysicalDevice Device();
		VkQueue presentQueue{}; //temp

	private:
		VkFormat GetSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VkPhysicalDevice physicalDevice{};
};