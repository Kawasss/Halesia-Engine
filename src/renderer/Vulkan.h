#pragma once
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <mutex>
#include <unordered_map>
#include <stdint.h>

#include "PhysicalDevice.h"

// for some reason VK_KHR_WIN32_SURFACE_EXTENSION_NAME throws an "undeclared identifier" error so this just redefines it
#undef VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#undef VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME
#undef VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#define VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME "VK_KHR_external_memory_win32"
#define VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME "VK_KHR_external_semaphore_win32"

#define nameof(s) #s
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define __STRLINE__ std::to_string(__LINE__)
#define CheckVulkanResult(message, result, function) if (result != VK_SUCCESS) throw VulkanAPIError(message, result, nameof(function), __FILENAME__, __STRLINE__)
#define LockLogicalDevice(logicalDevice) std::lock_guard<std::mutex> logicalDeviceLockGuard(Vulkan::FetchLogicalDeviceMutex(logicalDevice)) // can't create a function for this beacuse a lock guard gets destroyed when it goes out of scope

class Swapchain;
class Win32Window;
struct VulkanCreationObject;
typedef void* HANDLE;

const std::vector<const char*> validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

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
    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

class VulkanAPIError : public std::exception
{
public:
    VulkanAPIError(std::string message, VkResult result = VK_SUCCESS, std::string functionName = "", std::string file = "", std::string line = "");
    const char* what() const override{ return message.c_str(); }

private:
    std::string message;
};

class Vulkan
{
public:
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    static VkMemoryAllocateFlagsInfo*         optionalMemoryAllocationFlags;
    static std::mutex                         graphicsQueueMutex;
        
    static std::mutex&                        FetchLogicalDeviceMutex(VkDevice logicalDevice);
    static VkCommandPool                      FetchNewCommandPool(const VulkanCreationObject& creationObject);
    static void                               YieldCommandPool(uint32_t queueFamilyIndex, VkCommandPool commandPool);
    static void                               DestroyAllCommandPools(VkDevice logicalDevice);

    static std::vector<PhysicalDevice>        GetPhysicalDevices(VkInstance instance);
    static PhysicalDevice                     GetBestPhysicalDevice(std::vector<PhysicalDevice> devices, Surface surface);
    static PhysicalDevice                     GetBestPhysicalDevice(VkInstance instance, Surface surface);
  
    static VkInstance                         GenerateInstance();
    static std::vector<VkExtensionProperties> GetInstanceExtensions();
    static std::vector<VkExtensionProperties> GetLogicalDeviceExtensions(PhysicalDevice physicalDevice);
    static void                               ActivateLogicalDeviceExtensionFunctions(VkDevice logicalDevice, const std::vector<const char*>& logicalDeviceExtensions);

    static SwapChainSupportDetails            QuerySwapChainSupport(PhysicalDevice device, Surface surface);
    static VkSurfaceFormatKHR                 ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    static VkPresentModeKHR                   ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes);
    static VkExtent2D                         ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Win32Window* window);

    static void                               CreateImage(VkDevice logicalDevice, PhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateFlags flags, VkImage& image, VkDeviceMemory& memory);
    static VkImageView                        CreateImageView(VkDevice logicalDevice, VkImage image, VkImageViewType viewType, uint32_t mipLevels, uint32_t layerCount, VkFormat format, VkImageAspectFlags aspectFlags);

    static VkShaderModule                     CreateShaderModule(VkDevice logicalDevice, const std::vector<char>& code);
    static VkPipelineShaderStageCreateInfo    GetGenericShaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits shaderStageBit, const char* name = "main");

    static VkCommandBuffer                    BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool);
    static void                               EndSingleTimeCommands(VkDevice logicalDevice, VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);

    static VkDeviceAddress                    GetDeviceAddress(VkDevice logicalDevice, VkBuffer buffer);
    static VkDeviceAddress                    GetDeviceAddress(VkDevice logicalDevice, VkAccelerationStructureKHR accelerationStructure);
    static HANDLE                             GetWin32MemoryHandle(VkDevice logicalDevice, VkDeviceMemory memory);

    static uint32_t                           GetMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, PhysicalDevice physicalDevice);
    static bool                               HasStencilComponent(VkFormat format);

    static void                               CreateExternalBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    static void                               CreateBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    static void                               CopyBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue queue, VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size);

    static void                               PopulateDefaultViewport(VkViewport& viewport, Swapchain* swapchain);
    static void                               PopulateDefaultScissors(VkRect2D& scissors, Swapchain* swapchain);

private:
    static VkDebugUtilsMessengerEXT                                 debugMessenger;
    static std::mutex                                               commandPoolMutex;
    static std::unordered_map<uint32_t, std::vector<VkCommandPool>> queueCommandPools;
    static std::unordered_map<VkDevice, std::mutex>                 logicalDeviceMutexes;

    static VKAPI_ATTR VkBool32 VKAPI_CALL     DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    static void                               CreateDebugMessenger(VkInstance instance);
    static void                               DestroyDebugMessenger(VkInstance instance);

    static void                               CheckDeviceRequirements(bool indicesHasValue, bool extensionsSupported, bool swapChainIsCompatible, bool samplerAnisotropy, bool shaderUniformBufferArrayDynamicIndexing, std::set<std::string> unsupportedExtensions);
    static bool                               IsDeviceCompatible(PhysicalDevice device, Surface surface);
    static bool                               CheckInstanceExtensionSupport(std::vector<const char*> extensions);
    static bool                               CheckLogicalDeviceExtensionSupport(PhysicalDevice physicalDevice, const std::vector<const char*> extensions, std::set<std::string>& unsupportedExtensions);
    static bool                               CheckValidationSupport();
};