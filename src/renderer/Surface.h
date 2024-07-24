#pragma once
#include "vulkan/vulkan.h"

class Window;

class Surface
{
	public:
		static Surface	GenerateSurface(VkInstance instance, Window* window);
		VkSurfaceKHR	VkSurface();
		void			Destroy();

	private:
		VkSurfaceKHR surface;
		VkInstance instance;
};