export module Renderer.Surface;

import <vulkan/vulkan.h>;

import System.Window;

export class Surface
{
public:
	static Surface	GenerateSurface(VkInstance instance, Window* window);
	VkSurfaceKHR	VkSurface();
	void			Destroy();

private:
	VkSurfaceKHR surface;
	VkInstance instance;
};