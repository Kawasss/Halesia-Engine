#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "Window.h"
#include "PhysicalDevice.h"

class Swapchain //better to move the depth buffer inside the class
{
public:
    Swapchain() = default;
    Swapchain(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Win32Window* window);

    void Recreate(VkRenderPass renderPass);
    void Generate(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Win32Window* window);

    void Destroy();
    void CreateDepthBuffers();
    void CreateImageViews();
    void CreateFramebuffers(VkRenderPass renderPass);

    VkSwapchainKHR vkSwapchain{};
    std::vector<VkImage> images{};
    std::vector<VkImageView> imageViews{};
    std::vector<VkFramebuffer> framebuffers{};
    VkFormat format{};
    VkExtent2D extent{};
    VkImage depthImage;
    VkImageView depthImageView;
    VkDeviceMemory depthImageMemory;

private:
    VkDevice logicalDevice;
    PhysicalDevice physicalDevice;
    Surface surface;
    Win32Window* window;
};