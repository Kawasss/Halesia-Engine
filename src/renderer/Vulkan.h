#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <set>
#include <optional>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <stdint.h>

#include "CreationObjects.h"
#include "PhysicalDevice.h"

#define nameof(s) #s
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define __STRLINE__ std::to_string(__LINE__)
#define CheckVulkanResult(message, result, function) if (result != VK_SUCCESS) throw VulkanAPIError(message, result, nameof(function), __FILENAME__, __STRLINE__)

const std::vector<const char*> validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

// for some reason VK_KHR_WIN32_SURFACE_EXTENSION_NAME throws an "undeclared identifier" error so this just redefines it
#undef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"

inline std::vector<const char*> requiredInstanceExtensions =
{
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
};

inline std::vector<const char*> requiredLogicalDeviceExtensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
};

class VulkanAPIError : public std::exception
{
public:
    VulkanAPIError(std::string message, VkResult result = VK_SUCCESS, std::string functionName = "", std::string file = "", std::string line = "");
    const char* what() const override{ return message.c_str(); }

private:
    std::string message;
};
// the idea here is to use a different command pool per thread to avoid threading errors, command pools get reused / destroyed depending on how many are idle
class QueueCommandPoolStorage
{
public:
    QueueCommandPoolStorage() = default;
    QueueCommandPoolStorage(VkDevice logicalDevice, uint32_t queueIndex);
    ~QueueCommandPoolStorage();
    VkCommandPool GetNewCommandPool();
    void ReturnCommandPool(VkCommandPool commandPool);
    void DestroyIdleCommandPools();

    QueueCommandPoolStorage& operator=(const QueueCommandPoolStorage& oldStorage);

private:
    uint32_t queueIndex;
    VkDevice logicalDevice;
    std::vector<VkCommandPool> unusedCommandPools;
    std::mutex commandPoolStorageMutex;
};

class Vulkan
{
    public:
        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities{};
            std::vector<VkSurfaceFormatKHR> formats{};
            std::vector<VkPresentModeKHR> presentModes{};
        };

        static VkMemoryAllocateFlagsInfo* optionalMemoryAllocationFlags;
        static std::mutex graphicsQueueMutex;
        
        static VkCommandPool                FetchNewCommandPool(const VulkanCreationObject& creationObject);
        static void                         YieldCommandPool(uint32_t queueFamilyIndex, VkCommandPool commandPool);

        static SwapChainSupportDetails      QuerySwapChainSupport(PhysicalDevice device, Surface surface);
        static std::vector<PhysicalDevice>  GetPhysicalDevices(VkInstance instance);
        static VkInstance                   GenerateInstance();
        static PhysicalDevice               GetBestPhysicalDevice(std::vector<PhysicalDevice> devices, Surface surface);
        static PhysicalDevice               GetBestPhysicalDevice(VkInstance instance, Surface surface);
        static VkSurfaceFormatKHR           ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        static VkPresentModeKHR             ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
        static VkExtent2D                   ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Win32Window* window);
        static VkImageView                  CreateImageView(VkDevice logicalDevice, VkImage image, VkImageViewType viewType, uint32_t mipLevels, uint32_t layerCount, VkFormat format, VkImageAspectFlags aspectFlags);
        static VkShaderModule               CreateShaderModule(VkDevice logicalDevice, const std::vector<char>& code);
        static VkCommandBuffer              BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);
        static VkDeviceAddress              GetDeviceAddress(VkDevice logicalDevice, VkBuffer buffer);
        static VkDeviceAddress              GetDeviceAddress(VkDevice logicalDevice, VkAccelerationStructureKHR accelerationStructure);
        static uint32_t                     GetMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, PhysicalDevice physicalDevice);
        static bool                         HasStencilComponent(VkFormat format);
        static void                         CreateDebugMessenger(VkInstance instance);
        static void                         DestroyDebugMessenger(VkInstance instance);
        static void                         EndSingleTimeCommands(VkDevice logicalDevice, VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);
        static void                         CreateImage(VkDevice logicalDevice, PhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateFlags flags, VkImage& image, VkDeviceMemory& memory);
        static void                         CreateBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        static void                         CopyBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue queue, VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size);
        static void                         ActivateLogicalDeviceExtensionFunctions(VkDevice logicalDevice, const std::vector<const char*>& logicalDeviceExtensions);

        static VkPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties(VkPhysicalDevice device)
        {
            VkPhysicalDeviceMemoryProperties properties{};
            vkGetPhysicalDeviceMemoryProperties(device, &properties);
            return properties;
        }

        static std::vector<VkExtensionProperties> GetInstanceExtensions()
        {
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

            return extensions;
        }

        static std::vector <VkExtensionProperties> GetLogicalDeviceExtensions(PhysicalDevice physicalDevice)
        {
            uint32_t extensionCount = 0;
            vkEnumerateDeviceExtensionProperties(physicalDevice.Device(), nullptr, &extensionCount, nullptr);
            
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(physicalDevice.Device(), nullptr, &extensionCount, extensions.data());

            return extensions;
        }

        static VkPipelineShaderStageCreateInfo GetGenericShaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits shaderStageBit, const char* name = "main")
        {
            VkPipelineShaderStageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfo.stage = shaderStageBit;
            createInfo.module = module;
            createInfo.pName = name;

            return createInfo;
        }

    private:
        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

        static VkDebugUtilsMessengerEXT debugMessenger;
        static std::unordered_map<uint32_t, QueueCommandPoolStorage> queueCommandPoolStorages;

        static void CheckDeviceRequirements(bool indicesHasValue, bool extensionsSupported, bool swapChainIsCompatible, bool samplerAnisotropy, bool shaderUniformBufferArrayDynamicIndexing, std::set<std::string> unsupportedExtensions)
        {
            if (!indicesHasValue)
                throw VulkanAPIError("No compatible queue family could be found", VK_SUCCESS, nameof(!indices.HasValue()), __FILENAME__, __STRLINE__);

            else if (!extensionsSupported)
            {
                std::string message = "Failed to find support for one or more logical device extensions:\n";
                for (std::string extension : unsupportedExtensions)
                    message += '\n' + extension;
                throw VulkanAPIError(message, VK_SUCCESS, nameof(Vulkan::CheckLogicalDeviceExtensionSupport(device, requiredLogicalDeviceExtensions)), __FILENAME__, __STRLINE__);
            }

            else if (!swapChainIsCompatible)
                throw VulkanAPIError("No support for a swapchain could be found", VK_SUCCESS, nameof(Vulkan::QuerySwapChainSupport(device, surface)), __FILENAME__, __STRLINE__);

            else if (!samplerAnisotropy)
                throw VulkanAPIError("Critical feature is missing: VkPhysicalDeviceFeatures::samplerAnisotropy", VK_SUCCESS, nameof(device.Features().samplerAnisotropy), __FILENAME__, __STRLINE__);

            else if (!shaderUniformBufferArrayDynamicIndexing)
                throw VulkanAPIError("Critical feature is missing: VkPhysicalDeviceFeatures::shaderUniformBufferArrayDynamicIndexing", VK_SUCCESS, nameof(device.Features().shaderUniformBufferArrayDynamicIndexing), __FILENAME__, __STRLINE__);
        }

        static bool IsDeviceCompatible(PhysicalDevice device, Surface surface)
        {
            QueueFamilyIndices indices = device.QueueFamilies(surface);
            std::set<std::string> unsupportedExtensions;
            bool extensionsSupported = CheckLogicalDeviceExtensionSupport(device, requiredLogicalDeviceExtensions, unsupportedExtensions);

            bool swapChainIsCompatible = false;
            if (extensionsSupported)
            {
                SwapChainSupportDetails support = QuerySwapChainSupport(device, surface);
                swapChainIsCompatible = !support.formats.empty() && !support.presentModes.empty();
            }

            CheckDeviceRequirements(indices.HasValue(), extensionsSupported, swapChainIsCompatible, device.Features().samplerAnisotropy, device.Features().shaderUniformBufferArrayDynamicIndexing, unsupportedExtensions);

            return indices.HasValue() && extensionsSupported && swapChainIsCompatible && device.Features().samplerAnisotropy && device.Features().shaderUniformBufferArrayDynamicIndexing;
        }

        static bool CheckInstanceExtensionSupport(std::vector<const char*> extensions)
        {
            std::vector<VkExtensionProperties> allExtensions = GetInstanceExtensions();
            std::set<std::string> stringExtensions(extensions.begin(), extensions.end());
            for (const VkExtensionProperties& property : allExtensions)
                stringExtensions.erase(property.extensionName);
            
            return stringExtensions.empty();
        }

        static bool CheckLogicalDeviceExtensionSupport(PhysicalDevice physicalDevice, const std::vector<const char*> extensions, std::set<std::string>& unsupportedExtensions)
        {
            std::vector<VkExtensionProperties> allExtensions = GetLogicalDeviceExtensions(physicalDevice);
            unsupportedExtensions = std::set<std::string>(extensions.begin(), extensions.end());
            for (const VkExtensionProperties& property : allExtensions)
                unsupportedExtensions.erase(property.extensionName);

            return unsupportedExtensions.empty();
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