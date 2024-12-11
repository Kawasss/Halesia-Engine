#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "PhysicalDevice.h"
#include "VideoMemoryManager.h"
#include "surface.h"

class Window;

class Swapchain
{
public:
    Swapchain() = default;
    Swapchain(Surface surface, Window* window, bool vsync);
    ~Swapchain() { Destroy(); }

    void Recreate(bool vsync);
    void Recreate(VkRenderPass renderPass, bool vsync);
    void Generate(Surface surface, Window* window, bool vsync);

    void Destroy();
    void CreateDepthBuffers();
    void CreateImageViews();
    void CreateFramebuffers(VkRenderPass renderPass);

    void CopyImageToSwapchain(VkImage image, VkCommandBuffer commandBuffer, uint32_t currentImage);

    VkSwapchainKHR vkSwapchain{};
    std::vector<VkImage> images{};
    std::vector<VkImageView> imageViews{};
    std::vector<VkFramebuffer> framebuffers{};
    VkFormat format{};
    VkExtent2D extent{};
    VvmImage depthImage;
    VkImageView depthImageView;

private:
    VkDevice logicalDevice;
    PhysicalDevice physicalDevice;
    Surface surface;
    Window* window;
};