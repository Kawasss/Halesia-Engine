#include "renderer/RayTracing.h"
#include "renderer/Vulkan.h"

void RayTracing::Init(PhysicalDevice physicalDevice)
{
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingProperties{};
	rayTracingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

	VkPhysicalDeviceProperties2 properties2{};
	properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties2.pNext = &rayTracingProperties;

	vkGetPhysicalDeviceProperties2(physicalDevice.Device(), &properties2);
}