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
		uint64_t VRAM() const;
		uint64_t AdditionalRAM() const;
		VkDevice GetLogicalDevice(Surface& surface);
		QueueFamilyIndices QueueFamilies(Surface& surface) const;
		VkFormat GetDepthFormat() const;
		VkPhysicalDeviceProperties Properties() const;
		VkPhysicalDeviceMemoryProperties MemoryProperties() const;
		VkPhysicalDeviceFeatures Features() const;
		VkPhysicalDevice Device();
		VkQueue presentQueue{}; //temp

	private:
		VkFormat GetSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
		VkPhysicalDevice physicalDevice{};
};