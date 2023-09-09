#include <array>

#include <vulkan/vulkan.h>
#include "renderer/Swapchain.h"
#include "system/Window.h"
#include "renderer/Vulkan.h"
#include "renderer/PhysicalDevice.h"
#include "renderer/Surface.h"

Swapchain::Swapchain(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Win32Window* window)
{
    Generate(logicalDevice, physicalDevice, surface, window);
}

void Swapchain::Generate(VkDevice logicalDevice, PhysicalDevice physicalDevice, Surface surface, Win32Window* window)
{
    this->logicalDevice = logicalDevice;
    this->physicalDevice = physicalDevice;
    this->surface = surface;
    this->window = window;

    Vulkan::SwapChainSupportDetails swapchainSupport = Vulkan::QuerySwapChainSupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat = Vulkan::ChooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR presentMode = Vulkan::ChooseSwapPresentMode(swapchainSupport.presentModes);
    VkExtent2D extent2D = Vulkan::ChooseSwapExtent(swapchainSupport.capabilities, window);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount)
        imageCount = swapchainSupport.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface.VkSurface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent2D;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = physicalDevice.QueueFamilies(surface);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.presentFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = true;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &vkSwapchain) != VK_SUCCESS)
        throw std::runtime_error("Couldn't create a swapchain with the current logical device");

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(logicalDevice, vkSwapchain, &swapchainImageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice, vkSwapchain, &swapchainImageCount, images.data());

    extent = extent2D;
    format = surfaceFormat.format;
}

void Swapchain::Destroy()
{
    vkDestroyImageView(logicalDevice, depthImageView, nullptr);
    vkDestroyImage(logicalDevice, depthImage, nullptr);
    vkFreeMemory(logicalDevice, depthImageMemory, nullptr);

    for (const VkFramebuffer& framebuffer : framebuffers)
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);

    for (const VkImageView& imageView : imageViews)
        vkDestroyImageView(logicalDevice, imageView, nullptr);

    vkDestroySwapchainKHR(logicalDevice, vkSwapchain, nullptr);
}

void Swapchain::Recreate(VkRenderPass renderPass)
{
    int width = 0, height = 0;
    window->GetWindowDimensions(&width, &height);

    #ifndef NDEBUG
    if (width == 0 || height == 0)
        std::cout << "Window is minimized, waiting until it is maximized" << std::endl;
    #endif

    while (width == 0 || height == 0)
    {
        window->GetWindowDimensions(&width, &height);
        window->PollEvents();
    }

    vkDeviceWaitIdle(logicalDevice);

    Destroy();

    Generate(logicalDevice, physicalDevice, surface, window);
    CreateImageViews();
    CreateDepthBuffers();
    CreateFramebuffers(renderPass);
}

void Swapchain::CreateDepthBuffers()
{
    VkFormat depthFormat = physicalDevice.GetDepthFormat();
    Vulkan::CreateImage(logicalDevice, physicalDevice, extent.width, extent.height, 1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, depthImage, depthImageMemory);
    depthImageView = Vulkan::CreateImageView(logicalDevice, depthImage, VK_IMAGE_VIEW_TYPE_2D, 1, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Swapchain::CreateImageViews()
{
    imageViews.resize(this->images.size());
    for (uint32_t i = 0; i < images.size(); i++)
        imageViews[i] = Vulkan::CreateImageView(logicalDevice, images[i], VK_IMAGE_VIEW_TYPE_2D, 1, 1, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Swapchain::CreateFramebuffers(VkRenderPass renderPass)
{
    framebuffers.resize(imageViews.size());
    for (size_t i = 0; i < imageViews.size(); i++)
    {
        std::array<VkImageView, 2> attachments =
        {
            imageViews[i],
            depthImageView
        };
        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = renderPass;
        framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = extent.width;
        framebufferCreateInfo.height = extent.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(logicalDevice, &framebufferCreateInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create the framebuffers needed for the swapchain");
    }
}