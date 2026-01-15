#include <array>
#include <vulkan/vulkan.h>

#include "renderer/Swapchain.h"
#include "renderer/Vulkan.h"
#include "renderer/PhysicalDevice.h"
#include "renderer/VulkanAPIError.h"
#include "renderer/GarbageManager.h"
#include "renderer/Surface.h"

import System.Window;

Swapchain::Swapchain(Surface surface, Window* window, bool vsync)
{
    Generate(surface, window, vsync);
}

void Swapchain::Generate(Surface surface, Window* window, bool vsync)
{
    const Vulkan::Context& ctx = Vulkan::GetContext();

    this->logicalDevice = ctx.logicalDevice;
    this->physicalDevice = ctx.physicalDevice;
    this->surface = surface;
    this->window = window;

    Vulkan::SwapChainSupportDetails swapchainSupport = Vulkan::QuerySwapChainSupport(physicalDevice, surface.VkSurface());

    VkSurfaceFormatKHR surfaceFormat = Vulkan::ChooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR presentMode = Vulkan::ChooseSwapPresentMode(swapchainSupport.presentModes, vsync);
    VkExtent2D extent2D = Vulkan::ChooseSwapExtent(swapchainSupport.capabilities, window->GetWidth(), window->GetHeight());

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
    CheckVulkanResult("Couldn't create a swapchain with the current logical device", result);

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

    VkImageMemoryBarrier swapchainPresentBarrier = swapchainMemoryBarrier;
    swapchainPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    swapchainPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkImageMemoryBarrier copyMemoryBarrier = swapchainMemoryBarrier;
    copyMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    copyMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    copyMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyMemoryBarrier.image = image;

    VkImageMemoryBarrier writeBarrier = swapchainMemoryBarrier;
    writeBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    writeBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    writeBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    writeBarrier.image = image;

    VkImageCopy imageCopy{};
    imageCopy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.srcSubresource.layerCount = 1;
    imageCopy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopy.dstSubresource.layerCount = 1;
    imageCopy.srcOffset = { 0, 0, 0 };
    imageCopy.dstOffset = { 0, 0, 0 };
    imageCopy.extent = { extent.width, extent.height, 1 };

    VkImageMemoryBarrier firstBarriers[]  = { swapchainMemoryBarrier, copyMemoryBarrier };
    VkImageMemoryBarrier secondBarriers[] = { swapchainPresentBarrier, writeBarrier };

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, firstBarriers);

    vkCmdCopyImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, images[currentImage], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 2, secondBarriers);
}

void Swapchain::Destroy()
{
    vgm::Delete(depthImageView);
    depthImage.Destroy();

    for (const VkFramebuffer& framebuffer : framebuffers)
        vgm::Delete(framebuffer);

    for (const VkImageView& imageView : imageViews)
        vgm::Delete(imageView);

    vkDestroySwapchainKHR(logicalDevice, vkSwapchain, nullptr);
}

void Swapchain::Recreate(bool vsync)
{
    if (!window->CanBeRenderedTo())
        return;

    LockLogicalDevice(logicalDevice);
    vkDeviceWaitIdle(logicalDevice);

    Destroy();

    Generate(surface, window, vsync);
    CreateImageViews();
}

void Swapchain::Recreate(VkRenderPass renderPass, bool vsync)
{
    int width = window->GetWidth(), height = window->GetHeight();

    if (width == 0 || height == 0)
        return;

    Destroy();

    Generate(surface, window, vsync);
    CreateImageViews();
    CreateDepthBuffers();
    CreateFramebuffers(renderPass);
}

void Swapchain::CreateDepthBuffers()
{
    VkFormat depthFormat = physicalDevice.GetDepthFormat();
    depthImage = Vulkan::CreateImage(extent.width, extent.height, 1, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
    depthImageView = Vulkan::CreateImageView(depthImage.Get(), VK_IMAGE_VIEW_TYPE_2D, 1, 1, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Swapchain::CreateImageViews()
{
    imageViews.resize(this->images.size());
    for (uint32_t i = 0; i < images.size(); i++)
        imageViews[i] = Vulkan::CreateImageView(images[i], VK_IMAGE_VIEW_TYPE_2D, 1, 1, format, VK_IMAGE_ASPECT_COLOR_BIT);
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