#pragma once
#include "vulkan/vulkan.h"

class Win32Window;

class Surface
{
	public:
		static Surface GenerateSurface(VkInstance instance, Win32Window* window);
		VkSurfaceKHR VkSurface();
		void Destroy();

	private:
		VkSurfaceKHR surface;
		VkInstance instance;
};