#include "renderer/Vulkan.h"
#include "renderer/VulkanAPIError.h"
#include "renderer/Surface.h"
#include "system/Window.h"

Surface Surface::GenerateSurface(VkInstance instance, Window* window)
{
	Surface lSurface{};
	lSurface.instance = instance;

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo{};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hwnd = window->GetHandle();
	surfaceCreateInfo.hinstance = window->GetInstance();

	VkResult result = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &lSurface.surface);
	CheckVulkanResult("Failed to create a window surface for the current instance", result);

	return lSurface;
}

VkSurfaceKHR Surface::VkSurface()
{
	return surface;
}

void Surface::Destroy()
{
	vkDestroySurfaceKHR(instance, surface, nullptr);
}