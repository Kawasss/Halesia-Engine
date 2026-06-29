export module Renderer.Swapchain;

import std;

import System.Window;

import Renderer.Surface;
import Renderer.PhyiscalDevice;
import Renderer.VideoMemoryManager;

import <vulkan/vulkan.h>;

export class Swapchain
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
    vvm::Image depthImage;
    VkImageView depthImageView;

private:
    VkDevice logicalDevice;
    PhysicalDevice physicalDevice;
    Surface surface;
    Window* window;
};