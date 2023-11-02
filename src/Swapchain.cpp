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
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
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

    VkResult result = vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &vkSwapchain);
    if (result != VK_SUCCESS)
        throw VulkanAPIError("Couldn't create a swapchain with the current logical device", result, nameof(vkCreateSwapchainKHR), __FILENAME__, __STRLINE__);

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(logicalDevice, vkSwapchain, &swapchainImageCount, nullptr);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice, vkSwapchain, &swapchainImageCount, images.data());

    extent = extent2D;
    format = surfaceFormat.format;
}

void Swapchain::CopyImageToSwapchain(VkImage image, VkCommandBuffer commandBuffer, uint32_t currentImage)
{
    VkImageMemoryBarrier swapchainMemoryBarrier{};
    swapchainMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchainMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainMemoryBarrier.image = images[currentImage];
    swapchainMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainMemoryBarrier.subresourceRange.levelCount = 1;
    swapchainMemoryBarrier.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier swapchainPresentBarrier{};
    swapchainPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapchainPresentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapchainPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapchainPresentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainPresentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainPresentBarrier.image = images[currentImage];
    swapchainPresentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainPresentBarrier.subresourceRange.levelCount = 1;
    swapchainPresentBarrier.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier copyMemoryBarrier{};
    copyMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    copyMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    copyMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    copyMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copyMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    copyMemoryBarrier.image = image;
    copyMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyMemoryBarrier.subresourceRange.levelCount = 1;
    copyMemoryBarrier.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier writeBarrier{};
    writeBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    writeBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    writeBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    writeBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    writeBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    writeBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    writeBarrier.image = image;
    writeBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    writeBarrier.subresourceRange.levelCount = 1;
    writeBarrier.subresourceRange.layerCount = 1;

    VkImageCopy imageCopy{};
    imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.srcSubresource.layerCount = 1;
    imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.dstSubresource.layerCount = 1;
    imageCopy.srcOffset = { 0, 0, 0 };
    imageCopy.dstOffset = { 0, 0, 0 };
    imageCopy.extent = { extent.width, extent.height, 1 };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapchainMemoryBarrier);
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &copyMemoryBarrier);

    vkCmdCopyImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, images[currentImage], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapchainPresentBarrier);
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &writeBarrier);
}

void Swapchain::Destroy()
{
    /*vkDestroyImageView(logicalDevice, depthImageView, nullptr);
    vkDestroyImage(logicalDevice, depthImage, nullptr);
    vkFreeMemory(logicalDevice, depthImageMemory, nullptr);

    for (const VkFramebuffer& framebuffer : framebuffers)
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);*/

    for (const VkImageView& imageView : imageViews)
        vkDestroyImageView(logicalDevice, imageView, nullptr);

    vkDestroySwapchainKHR(logicalDevice, vkSwapchain, nullptr);
}

void Swapchain::Recreate() // hopefully a temporary fix, used for ray tracing
{
    int width = window->GetWidth(), height = window->GetHeight();
    
#ifndef NDEBUG
    if (width == 0 || height == 0)
        std::cout << "Window is minimized, waiting until it is maximized" << std::endl;
#endif

    while (width == 0 || height == 0)
    {
        width = window->GetWidth();
        height = window->GetHeight();
        window->PollMessages();
    }

    vkDeviceWaitIdle(logicalDevice);

    Destroy();

    Generate(logicalDevice, physicalDevice, surface, window);
    CreateImageViews();
}

void Swapchain::Recreate(VkRenderPass renderPass)
{
    int width = window->GetWidth(), height = window->GetHeight();

    #ifndef NDEBUG
    if (width == 0 || height == 0)
        std::cout << "Window is minimized, waiting until it is maximized" << std::endl;
    #endif

    while (width == 0 || height == 0)
    {
        width = window->GetWidth();
        height = window->GetHeight();
        window->PollMessages();
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