#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <mutex>
#include <map>

#include "PhysicalDevice.h"
#include "CommandBuffer.h"
#include "VideoMemoryManager.h"

#include "../system/CriticalSection.h"

#define nameof(s) #s
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define CheckVulkanResult(message, result, function) if (result != VK_SUCCESS) throw VulkanAPIError(message, result, nameof(function), __FILENAME__, __LINE__)
#define LockLogicalDevice(logicalDevice) std::lock_guard<std::mutex> logicalDeviceLockGuard(Vulkan::FetchLogicalDeviceMutex(logicalDevice)) // can't create a function for this beacuse a lock guard gets destroyed when it goes out of scope
#undef CreateSemaphore

struct VulkanCreationObject;
typedef void* HANDLE;

class VulkanAPIError : public std::exception
{
public:
    VulkanAPIError(std::string message, VkResult result = VK_SUCCESS, std::string functionName = "", std::string file = "", int line = 0);
    const char* what() const override{ return message.c_str(); }

private:
    std::string message;
};

class Vulkan
{
public:
    struct Context
    {
        VkInstance instance;
        VkDevice logicalDevice;
        PhysicalDevice physicalDevice;
        VkQueue graphicsQueue;
        uint32_t graphicsIndex;
        VkQueue presentQueue;
        uint32_t presentIndex;
        VkQueue computeQueue;
        uint32_t computeIndex;

        bool IsValid() const;
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    static VkMemoryAllocateFlagsInfo* optionalMemoryAllocationFlags;

    static win32::CriticalSection graphicsQueueSection;

    static VkDeviceSize allocatedMemory;

    static void                               DisableValidationLayers();

    static std::mutex&                        FetchLogicalDeviceMutex(VkDevice logicalDevice);
    static VkCommandPool                      FetchNewCommandPool(uint32_t queueIndex);
    static void                               YieldCommandPool(uint32_t queueFamilyIndex, VkCommandPool commandPool);
    static void                               DestroyAllCommandPools();

    static std::vector<PhysicalDevice>        GetPhysicalDevices(VkInstance instance);
    static PhysicalDevice                     GetBestPhysicalDevice(std::vector<PhysicalDevice> devices, Surface surface);
    static PhysicalDevice                     GetBestPhysicalDevice(VkInstance instance, Surface surface);
    static void                               ForcePhysicalDevice(const std::string& name); // forces 'GetBestPhysicalDevice' to return the GPU with the given name
  
    static VkInstance                         GenerateInstance();
    static std::vector<VkExtensionProperties> GetInstanceExtensions();
    static std::vector<VkExtensionProperties> GetLogicalDeviceExtensions(PhysicalDevice physicalDevice);

    static SwapChainSupportDetails            QuerySwapChainSupport(PhysicalDevice device, VkSurfaceKHR surface);
    static VkSurfaceFormatKHR                 ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    static VkPresentModeKHR                   ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes, bool vsync = false);
    static VkExtent2D                         ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);

    static vvm::Image                         CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateFlags flags, VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED);
    static VkImageView                        CreateImageView(VkImage image, VkImageViewType viewType, uint32_t mipLevels, uint32_t layerCount, VkFormat format, VkImageAspectFlags aspectFlags);
    static void                               TransitionColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags src, VkAccessFlags dst, VkPipelineStageFlags srcPipe, VkPipelineStageFlags dstPipe); // no mipmaps or layers !!

    static VkShaderModule                     CreateShaderModule(const std::vector<char>& code);
    static VkPipelineShaderStageCreateInfo    GetGenericShaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits shaderStageBit, const char* name = "main");

    static VkCommandBuffer                    BeginSingleTimeCommands(VkCommandPool commandPool);
    static void                               EndSingleTimeCommands(VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);

    static void                               AllocateCommandBuffers(const VkCommandBufferAllocateInfo& allocationInfo, std::vector<CommandBuffer>& commandBuffers);

    static VkDeviceAddress                    GetDeviceAddress(VkBuffer buffer);
    static VkDeviceAddress                    GetDeviceAddress(VkAccelerationStructureKHR accelerationStructure);
    static HANDLE                             GetWin32MemoryHandle(VkDeviceMemory memory);

    static uint32_t                           GetMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, PhysicalDevice physicalDevice);
    static bool                               HasStencilComponent(VkFormat format);
    static bool                               LogicalDeviceExtensionIsSupported(PhysicalDevice physicalDevice, const char* extension);
    static bool                               InstanceExtensionIsSupported(const char* extension);

    static void                               CreateExternalBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    static vvm::Buffer                        CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    static void                               CopyBuffer(VkCommandPool commandPool, VkQueue queue, VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size);

    static void                               PopulateDefaultViewport(VkViewport& viewport, VkExtent2D extents);
    static void                               PopulateDefaultScissors(VkRect2D& scissors, VkExtent2D extents);

    static VkPipelineViewportStateCreateInfo  GetDefaultViewportStateCreateInfo(VkViewport& viewport, VkRect2D& scissors, VkExtent2D extents);
    static VkPipelineViewportStateCreateInfo  GetDynamicViewportStateCreateInfo();
    static VkPipelineDynamicStateCreateInfo   GetDynamicStateCreateInfo(std::vector<VkDynamicState>& dynamicStates);
    static VkPipelineDynamicStateCreateInfo   GetDynamicStateCreateInfo();

    static void                               InitializeContext(Context context);
    static const Context&                     GetContext();

    static VkQueryPool                        CreateQueryPool(VkQueryType type, uint32_t amount);
    static std::vector<uint64_t>              GetQueryPoolResults(VkQueryPool queryPool, uint32_t amount, uint32_t offset = 0);

    //static void                               SubmitObjectForDeletion(std::function<void()>&& func); // removed for a different implementation
    static void                               DeleteSubmittedObjects();

    static void                               StartDebugLabel(VkCommandBuffer commandBuffer, const std::string& label);
    static void                               InsertDebugLabel(VkCommandBuffer commandBuffer, const std::string& label);

    static VkFence                            CreateFence(VkFenceCreateFlags flags = 0, void* pNext = nullptr);
    static VkSemaphore                        CreateSemaphore(void* pNext = nullptr);

    template<typename Type> static void       SetDebugName(Type object, const char* name);

    static void                               AddInstanceExtension(const char* name);
    static void                               AddDeviceExtenion(const char* name);
    static void                               AddValidationLayer(const char* name);

    static void                               AddDynamicState(VkDynamicState state);
    static void                               RemoveDynamicState(VkDynamicState state);

    static std::vector<const char*>&          GetDeviceExtensions() { return requiredLogicalDeviceExtensions; }
    static std::vector<VkDynamicState>&       GetDynamicStates()    { return dynamicStates; }

    static void                               Destroy();

private:
    static Context context;

    static std::map<uint32_t, std::vector<VkCommandPool>> queueCommandPools;
    static std::map<VkDevice, std::mutex>                 logicalDeviceMutexes;

    static std::vector<const char*> requiredLogicalDeviceExtensions;
    static std::vector<const char*> requiredInstanceExtensions;
    static std::vector<const char*> validationLayers;

    static std::vector<VkDynamicState> dynamicStates;

    static VKAPI_ATTR VkBool32 VKAPI_CALL     DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    static void                               CreateBufferHandle(VkBuffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage, void* pNext = nullptr);
    static void                               AllocateMemory(VkDeviceMemory& memory, VkMemoryRequirements& memoryRequirements, VkMemoryPropertyFlags properties, void* pNext = nullptr);
    static void                               GetDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static bool                               IsDeviceCompatible(PhysicalDevice device, Surface surface);
    static bool                               CheckInstanceExtensionSupport(std::vector<const char*> extensions);
    static bool                               CheckLogicalDeviceExtensionSupport(PhysicalDevice physicalDevice, const std::vector<const char*> extensions, std::set<std::string>& unsupportedExtensions);
    static bool                               CheckValidationSupport();

    static void                               DebugNameObject(uint64_t object, VkObjectType type, const char* name);
};

template<typename Type>
void Vulkan::SetDebugName(Type object, const char* name)
{
    throw VulkanAPIError((std::string)"Cannot set the name of object: unsupported type " + typeid(Type).name());
}

template<>
inline void Vulkan::SetDebugName<VkRenderPass>(VkRenderPass object, const char* name)
{
    DebugNameObject(reinterpret_cast<uint64_t>(object), VK_OBJECT_TYPE_RENDER_PASS, name);
}

template<>
inline void Vulkan::SetDebugName<VkFramebuffer>(VkFramebuffer object, const char* name)
{
    DebugNameObject(reinterpret_cast<uint64_t>(object), VK_OBJECT_TYPE_FRAMEBUFFER, name);
}

template<>
inline void Vulkan::SetDebugName<VkBuffer>(VkBuffer object, const char* name)
{
    DebugNameObject(reinterpret_cast<uint64_t>(object), VK_OBJECT_TYPE_BUFFER, name);
}

template<>
inline void Vulkan::SetDebugName<VkImage>(VkImage object, const char* name)
{
    DebugNameObject(reinterpret_cast<uint64_t>(object), VK_OBJECT_TYPE_IMAGE, name);
}

template<>
inline void Vulkan::SetDebugName<VkCommandBuffer>(VkCommandBuffer object, const char* name)
{
    DebugNameObject(reinterpret_cast<uint64_t>(object), VK_OBJECT_TYPE_COMMAND_BUFFER, name);
}

#define VULKAN_TRACK_MEMORY
#ifdef  VULKAN_TRACK_MEMORY

inline std::map<VkDeviceMemory, VkDeviceSize> memoryToSize;

inline VkResult vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory)
{
    static PFN_vkAllocateMemory fnPtr = (PFN_vkAllocateMemory)vkGetDeviceProcAddr(device, "vkAllocateMemory");

    VkResult result = fnPtr(device, pAllocateInfo, pAllocator, pMemory);
    memoryToSize[*pMemory] = pAllocateInfo->allocationSize;

    Vulkan::allocatedMemory += pAllocateInfo->allocationSize;

    return result;
}

inline void vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator)
{
    static PFN_vkFreeMemory fnPtr = (PFN_vkFreeMemory)vkGetDeviceProcAddr(device, "vkFreeMemory");

    fnPtr(device, memory, pAllocator);

    Vulkan::allocatedMemory -= memoryToSize[memory];
    memoryToSize.erase(memory);
}

#endif