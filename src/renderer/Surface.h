#pragma once

#include "system/Window.h"

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