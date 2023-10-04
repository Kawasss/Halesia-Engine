#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <set>
#include <optional>
#include <iostream>
#include <mutex>

#include "PhysicalDevice.h"

const std::vector<const char*> validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

// for some reason VK_KHR_WIN32_SURFACE_EXTENSION_NAME throws an "undeclared identifier" error so this just redefines it
#undef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"

const std::vector<const char*> requiredInstanceExtensions =
{
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
};

const std::vector<const char*> requiredLogicalDeviceExtensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

class Vulkan
{
    public:
        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        static std::mutex* globalThreadingMutex;

        static SwapChainSupportDetails      QuerySwapChainSupport(PhysicalDevice device, Surface surface);
        static std::vector<PhysicalDevice>  GetPhysicalDevices(VkInstance instance);
        static VkInstance                   GenerateInstance();
        static PhysicalDevice               GetBestPhysicalDevice(std::vector<PhysicalDevice> devices, Surface surface);
        static PhysicalDevice               GetBestPhysicalDevice(VkInstance instance, Surface surface);
        static VkSurfaceFormatKHR           ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        static VkPresentModeKHR             ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
        static VkExtent2D                   ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Win32Window* window);
        static VkImageView                  CreateImageView(VkDevice logicalDevice, VkImage image, VkImageViewType viewType, uint32_t mipLevels, uint32_t layerCount, VkFormat format, VkImageAspectFlags aspectFlags);
        static VkCommandBuffer              BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);
        static void                         EndSingleTimeCommands(VkDevice logicalDevice, VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);
        static bool                         HasStencilComponent(VkFormat format);
        static void                         CreateImage(VkDevice logicalDevice, PhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateFlags flags, VkImage& image, VkDeviceMemory& memory);
        static uint32_t                     GetMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, PhysicalDevice physicalDevice);
        static void                         CreateBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        static void                         CopyBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue queue, VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size);
        

        static VkPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties(VkPhysicalDevice device)
        {
            VkPhysicalDeviceMemoryProperties properties{};
            vkGetPhysicalDeviceMemoryProperties(device, &properties);
            return properties;
        }

        static std::vector<VkExtensionProperties> GetAvaibleExtensions()
        {
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

            return extensions;
        }

        static VkPipelineShaderStageCreateInfo GetGenericVertexShaderCreateInfo(VkShaderModule shader)
        {
            VkPipelineShaderStageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            createInfo.module = shader;
            createInfo.pName = "main";

            return createInfo;
        }

        static VkPipelineShaderStageCreateInfo GetGenericFragmentShaderCreateInfo(VkShaderModule shader)
        {
            VkPipelineShaderStageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            createInfo.module = shader;
            createInfo.pName = "main";

            return createInfo;
        }

    private:
        static std::mutex graphicsQueueThreadingMutex;

        static bool IsDeviceCompatible(PhysicalDevice device, Surface surface)
        {
            //VkPhysicalDeviceProperties properties = GetPhyiscalDeviceProperties(device);
            QueueFamilyIndices indices = device.QueueFamilies(surface);
            bool extensionsSupported = CheckExtensionSupport(device);

            bool swapChainIsCompatible = false;
            if (extensionsSupported)
            {
                SwapChainSupportDetails support = QuerySwapChainSupport(device, surface);
                swapChainIsCompatible = !support.formats.empty() && !support.presentModes.empty();
            }

            return indices.HasValue() && extensionsSupported && swapChainIsCompatible && device.Features().samplerAnisotropy && device.Features().shaderUniformBufferArrayDynamicIndexing;
        }
        
        static bool CheckExtensionSupport(PhysicalDevice device)
        {
            std::vector<const char*> rExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device.Device(), nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensionProperties(extensionCount);
            vkEnumerateDeviceExtensionProperties(device.Device(), nullptr, &extensionCount, extensionProperties.data());

            std::set<std::string> extensions(rExtensions.begin(), rExtensions.end());
            for (const VkExtensionProperties& property : extensionProperties)
                extensions.erase(property.extensionName);

            return extensions.empty();
        }

        static bool CheckGivenExtensionSupport(std::vector<const char*> extensions)
        {
            std::vector<VkExtensionProperties> allExtensions = GetAvaibleExtensions();
            std::set<std::string> stringExtensions(extensions.begin(), extensions.end());
            for (const VkExtensionProperties& property : allExtensions)
                stringExtensions.erase(property.extensionName);
            
            return stringExtensions.empty();
        }

        static bool CheckValidationSupport()
        {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> layerProperties(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());

            for (const char* layerName : validationLayers)
            {
                bool foundLayer = false;

                for (const VkLayerProperties& layerProperty : layerProperties)
                    if (strcmp(layerName, layerProperty.layerName) == 0)
                    {
                        foundLayer = true;
                        break;
                    }
                if (!foundLayer)
                    return false;
            }
            return true;
        }
};