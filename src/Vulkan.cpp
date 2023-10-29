#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
#define NOMINMAX
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>
#include <unordered_map>

#include "renderer/Vulkan.h"
#include "Console.h"

#define nameof(s) #s
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define __STRLINE__ std::to_string(__LINE__)

#pragma region VulkanPointerFunctions
PFN_vkGetBufferDeviceAddressKHR pvkGetBufferDeviceAddressKHR = nullptr;
PFN_vkCreateRayTracingPipelinesKHR pvkCreateRayTracingPipelinesKHR = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR pvkGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkCreateAccelerationStructureKHR pvkCreateAccelerationStructureKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR pvkDestroyAccelerationStructureKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR pvkGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR pvkCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR pvkGetRayTracingShaderGroupHandlesKHR = nullptr;
PFN_vkCmdTraceRaysKHR pvkCmdTraceRaysKHR = nullptr;
#pragma endregion VulkanPointerFunctions

VkDebugUtilsMessengerEXT Vulkan::debugMessenger;

std::unordered_map<uint32_t, QueueCommandPoolStorage> Vulkan::queueCommandPoolStorages;

std::mutex Vulkan::graphicsQueueThreadingMutex;
std::mutex* Vulkan::graphicsQueueMutex = &graphicsQueueThreadingMutex;
VkMemoryAllocateFlagsInfo* Vulkan::optionalMemoryAllocationFlags = nullptr;

QueueCommandPoolStorage::QueueCommandPoolStorage(VkDevice logicalDevice, uint32_t queueIndex) 
{ 
    this->queueIndex = queueIndex; 
    this->logicalDevice = logicalDevice; 
}


VkCommandPool QueueCommandPoolStorage::GetNewCommandPool()
{
    VkCommandPool commandPool;

    if (unusedCommandPools.size() == 0)
    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = queueIndex;

        VkResult result = vkCreateCommandPool(logicalDevice, &createInfo, nullptr, &commandPool);
        CheckVulkanResult("Failed to create a command pool for the storage buffer", result, vkCreateCommandPool);

#ifdef _DEBUG
        Console::WriteLine("Created a new command pool for queue index " + std::to_string(queueIndex), MESSAGE_SEVERITY_DEBUG);
#endif
    }
    else
    {
        commandPoolStorageMutex.lock();
        commandPool = unusedCommandPools[unusedCommandPools.size() - 1];
        unusedCommandPools.erase(unusedCommandPools.end() - 1);
        commandPoolStorageMutex.unlock();

#ifdef _DEBUG
        Console::WriteLine("Reused an existing command pool for queue index " + std::to_string(queueIndex) + ", amount left idle: " + std::to_string(unusedCommandPools.size()), MESSAGE_SEVERITY_DEBUG);
#endif
        if (unusedCommandPools.size() > 16)
            DestroyIdleCommandPools();
    }
    return commandPool;
}

void QueueCommandPoolStorage::ReturnCommandPool(VkCommandPool commandPool)
{
    commandPoolStorageMutex.lock();
    unusedCommandPools.push_back(commandPool);
    commandPoolStorageMutex.unlock();
}

VkCommandPool Vulkan::FetchNewCommandPool(const VulkanCreationObject& creationObject)
{
    if (queueCommandPoolStorages.count(creationObject.queueIndex) <= 0)
    {
        QueueCommandPoolStorage newStorage{ creationObject.logicalDevice, creationObject.queueIndex };
        queueCommandPoolStorages[creationObject.queueIndex] = newStorage;
#ifdef _DEBUG
        Console::WriteLine("Created a new command pool storage for queue family index " + std::to_string(creationObject.queueIndex), MESSAGE_SEVERITY_DEBUG);
#endif
    }
    return queueCommandPoolStorages[creationObject.queueIndex].GetNewCommandPool();
}

VkDeviceAddress Vulkan::GetDeviceAddress(VkDevice logicalDevice, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;

    return  vkGetBufferDeviceAddress(logicalDevice, &addressInfo);
}

void Vulkan::YieldCommandPool(uint32_t index, VkCommandPool commandPool)
{
    if (queueCommandPoolStorages.count(index) <= 0)
        throw VulkanAPIError("Failed to yield a command pool, no matching queue family index could be found", VK_SUCCESS, __FUNCTION__, __FILENAME__, __STRLINE__);
    queueCommandPoolStorages[index].ReturnCommandPool(commandPool);
}

QueueCommandPoolStorage::~QueueCommandPoolStorage()
{
    DestroyIdleCommandPools();
}

void QueueCommandPoolStorage::DestroyIdleCommandPools()
{
    commandPoolStorageMutex.lock();
    for (const VkCommandPool& commandPool : unusedCommandPools)
        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    unusedCommandPools.clear();
    commandPoolStorageMutex.unlock();
}

QueueCommandPoolStorage& QueueCommandPoolStorage::operator=(const QueueCommandPoolStorage& oldStorage)
{
    this->logicalDevice = oldStorage.logicalDevice;
    this->queueIndex = oldStorage.queueIndex;
    this->unusedCommandPools = oldStorage.unusedCommandPools;
    return *this;
}

VkShaderModule Vulkan::CreateShaderModule(VkDevice logicalDevice, const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;

    VkResult result = vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &module);
    if (result != VK_SUCCESS)
        throw VulkanAPIError("Failed to create a shader module", result, nameof(vkCreateShaderModule), __FILENAME__, __STRLINE__);
    return module;
}

PhysicalDevice Vulkan::GetBestPhysicalDevice(VkInstance instance, Surface surface)
{
    std::vector<PhysicalDevice> devices = GetPhysicalDevices(instance);
    return Vulkan::GetBestPhysicalDevice(devices, surface);
}

void Vulkan::CopyBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue queue, VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size)
{
    VkCommandBuffer localCommandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;

    vkCmdCopyBuffer(localCommandBuffer, sourceBuffer, destinationBuffer, 1, &copyRegion);

    Vulkan::EndSingleTimeCommands(logicalDevice, queue, localCommandBuffer, commandPool);
}

std::mutex endCommandMutex;
void Vulkan::EndSingleTimeCommands(VkDevice logicalDevice, VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool)
{
    VkResult result = vkEndCommandBuffer(commandBuffer);
    CheckVulkanResult("Failed to end the single time command buffer", result, vkQueueSubmit);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    endCommandMutex.lock();
    result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    CheckVulkanResult("Failed to submit the single time commands queue", result, vkQueueSubmit);
    
    result = vkQueueWaitIdle(queue);
    CheckVulkanResult("Failed to wait for the queue idle", result, vkQueueWaitIdle);

    endCommandMutex.unlock();
    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}

VkCommandBuffer Vulkan::BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandPool = commandPool;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer localCommandBuffer;
    vkAllocateCommandBuffers(logicalDevice, &allocateInfo, &localCommandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(localCommandBuffer, &beginInfo);
    CheckVulkanResult("Failed to begin single time commands", result, vkBeginCommandBuffer);

    return localCommandBuffer;
}

void Vulkan::CreateBuffer(VkDevice logicalDevice, PhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(logicalDevice, &createInfo, nullptr, &buffer);
    if (result != VK_SUCCESS)
        throw VulkanAPIError("Failed to create a new buffer", result, nameof(vkCreateBuffer), __FILENAME__, __STRLINE__);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = optionalMemoryAllocationFlags;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = Vulkan::GetMemoryType(memoryRequirements.memoryTypeBits, properties, physicalDevice);
    
    result = vkAllocateMemory(logicalDevice, &allocateInfo, nullptr, &bufferMemory);
    if (result != VK_SUCCESS)
        throw VulkanAPIError("Failed to allocate " + std::to_string(memoryRequirements.size) + " bytes of memory", result, nameof(vkAllocateMemory), __FILENAME__, __STRLINE__);

    #ifndef NDEBUG
        Console::WriteLine("Created a buffer with " + std::to_string(size) + " bytes");
    #endif

    vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

uint32_t Vulkan::GetMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, PhysicalDevice physicalDevice)
{
    VkPhysicalDeviceMemoryProperties physicalMemoryProperties = GetPhysicalDeviceMemoryProperties(physicalDevice.Device());

    for (uint32_t i = 0; i < physicalMemoryProperties.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (physicalMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    throw VulkanAPIError("Failed to get the memory type " + (std::string)string_VkMemoryPropertyFlags(properties) + " for the physical device", VK_SUCCESS, nameof(GetMemoryType), __FILENAME__, __STRLINE__);
}

void Vulkan::CreateImage(VkDevice logicalDevice, PhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateFlags flags, VkImage& image, VkDeviceMemory& memory)
{
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = mipLevels;
    createInfo.arrayLayers = arrayLayers;
    createInfo.format = format;
    createInfo.tiling = tiling;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.flags = flags;

    VkResult result = vkCreateImage(logicalDevice, &createInfo, nullptr, &image);
    if (result != VK_SUCCESS)
        throw VulkanAPIError("Failed to create an image", result, nameof(vkCreateImage), __FILENAME__, __STRLINE__);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = GetMemoryType(memoryRequirements.memoryTypeBits, properties, physicalDevice);

    result = vkAllocateMemory(logicalDevice, &allocateInfo, nullptr, &memory);
    if (result != VK_SUCCESS)
        throw VulkanAPIError("Failed to allocate memory for the image", result, nameof(vkAllocateMemory), __FILENAME__, __STRLINE__);

    vkBindImageMemory(logicalDevice, image, memory, 0);
}

bool Vulkan::HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkImageView Vulkan::CreateImageView(VkDevice logicalDevice, VkImage image, VkImageViewType viewType, uint32_t mipLevels, uint32_t layerCount, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = viewType;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspectFlags;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = mipLevels;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = layerCount;

    VkImageView imageView;

    VkResult result = vkCreateImageView(logicalDevice, &createInfo, nullptr, &imageView);
    if (result != VK_SUCCESS)
        throw VulkanAPIError("Failed to create an image view", result, nameof(vkCreateImageView), __FILENAME__, __STRLINE__);
    return imageView;
}

VkSurfaceFormatKHR Vulkan::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const VkSurfaceFormatKHR& format : availableFormats)
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    return availableFormats[0];
}

VkPresentModeKHR Vulkan::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
    for (const VkPresentModeKHR presentMode : presentModes)
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return presentMode;
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Vulkan::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabitilies, Win32Window* window)
{
    if (capabitilies.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabitilies.currentExtent;

    int width = window->GetWidth(), height = window->GetHeight();
    
    VkExtent2D extent =
    {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };
    extent.width = std::clamp(extent.width, capabitilies.minImageExtent.width, capabitilies.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabitilies.minImageExtent.height, capabitilies.maxImageExtent.width);
    
    return extent;
}

PhysicalDevice Vulkan::GetBestPhysicalDevice(std::vector<PhysicalDevice> devices, Surface surface)
{
    for (size_t i = 0; i < devices.size(); i++)
        if (IsDeviceCompatible(devices[i], surface)) //delete all of the unnecessary physical devices from ram
        {
            
#ifdef _DEBUG
            VkPhysicalDeviceProperties properties = devices[i].Properties();
            std::cout << "\nBest available physical device: " << properties.deviceName << "\n  type: " << string_VkPhysicalDeviceType(properties.deviceType) << "\n  driver version: " << properties.driverVersion << "\n  API version: " << properties.apiVersion << "\n  heap 0 total memory (VRAM): " << devices[i].VRAM() / (1024.0f * 1024.0f) << " MB\n" << std::endl;
#endif
            return devices[i];
        }
            

    std::string message = "There is no compatible vulkan GPU for this engine present: iterated through " + std::to_string(devices.size()) + " physical devices: \n";
    for (PhysicalDevice physicalDevice : devices)
        message += (std::string)physicalDevice.Properties().deviceName + "\n";
    throw VulkanAPIError(message, VK_SUCCESS, nameof(GetBestPhysicalDevice), __FILENAME__, __STRLINE__);
}

Vulkan::SwapChainSupportDetails Vulkan::QuerySwapChainSupport(PhysicalDevice device, Surface surface)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.Device(), surface.VkSurface(), &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.Device(), surface.VkSurface(), &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.Device(), surface.VkSurface(), &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.Device(), surface.VkSurface(), &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.Device(), surface.VkSurface(), &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkInstance Vulkan::GenerateInstance()
{
    if (enableValidationLayers && !CheckValidationSupport())
        throw VulkanAPIError("Failed the enable the required validation layers", VK_SUCCESS, nameof(GenerateInstance), __FILENAME__, __STRLINE__);

    VkInstance instance;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Halesia";
    appInfo.applicationVersion = VK_VERSION_1_3;
    appInfo.pEngineName = "Halesia";
    appInfo.engineVersion = VK_VERSION_1_3;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;   

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    if (enableValidationLayers)
    {
        requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = DebugCallback;

        createInfo.pNext = &debugCreateInfo;
    }
    else
        createInfo.enabledLayerCount = 0;

    createInfo.enabledExtensionCount = (uint32_t)requiredInstanceExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredInstanceExtensions.data();

#ifdef _DEBUG
    std::cout << "Enabled instance extensions:" << std::endl;
    for (const char* extension : requiredInstanceExtensions)
        std::cout << "  " + (std::string)extension << std::endl;
#endif

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS)
        throw VulkanAPIError("Couldn't create a Vulkan instance", result, nameof(vkCreateInstance), __FILENAME__, __STRLINE__);

    if (enableValidationLayers)
        CreateDebugMessenger(instance);

    return instance;
}

void Vulkan::CreateDebugMessenger(VkInstance instance)
{
    if (!enableValidationLayers)
        return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    PFN_vkCreateDebugUtilsMessengerEXT pvkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    VkResult result = pvkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger);
    CheckVulkanResult("Failed to create the debug messenger", result, nameof(vkCreateDebugUtilsMessengerEXT));

}

void Vulkan::DestroyDebugMessenger(VkInstance instance)
{
    if (!enableValidationLayers)
        return;

    PFN_vkDestroyDebugUtilsMessengerEXT pvkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (pvkDestroyDebugUtilsMessengerEXT != nullptr)
        pvkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
}

std::vector<PhysicalDevice> Vulkan::GetPhysicalDevices(VkInstance instance)
{
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
        throw VulkanAPIError("No Vulkan compatible GPUs could be found", VK_SUCCESS, nameof(GetPhysicalDevices), __FILENAME__, __STRLINE__);

    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

    std::vector<PhysicalDevice> devices;
    for (VkPhysicalDevice device : physicalDevices)
        devices.push_back(PhysicalDevice(device));
    
    return devices;
}

VkBool32 Vulkan::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    if (messageSeverity <= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        return VK_FALSE;
    //else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    //    throw VulkanAPIError("Validation error found, showing raw error data.\n" + (std::string)pCallbackData->pMessage);

    std::cout << "Validation layer message:\n" << pCallbackData->pMessage << "\nseverity: " << string_VkDebugUtilsMessageSeverityFlagBitsEXT(messageSeverity) << '\n' << "type: " << string_VkDebugUtilsMessageTypeFlagsEXT(messageType)  << '\n' << std::endl;
    return VK_TRUE;
}

void Vulkan::ActivateLogicalDeviceExtensionFunctions(VkDevice logicalDevice, const std::vector<const char*>& logicalDeviceExtensions)
{
    for (const std::string logicalDeviceExtension : logicalDeviceExtensions) // no switch case because c++ cant handle strings in switch cases
    {
        if (logicalDeviceExtension == VK_KHR_SWAPCHAIN_EXTENSION_NAME) continue;
        else if (logicalDeviceExtension == VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) continue;
        else if (logicalDeviceExtension == VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) continue;

        else if (logicalDeviceExtension == VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME)
        {
            pvkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(logicalDevice, "vkCmdBuildAccelerationStructuresKHR");
            pvkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(logicalDevice, "vkCreateAccelerationStructureKHR");
            pvkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(logicalDevice, "vkDestroyAccelerationStructureKHR");
            pvkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(logicalDevice, "vkGetAccelerationStructureBuildSizesKHR");
            pvkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR");
        }

        else if (logicalDeviceExtension == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
        {
            pvkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(logicalDevice, "vkCmdTraceRaysKHR");
            pvkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(logicalDevice, "vkCreateRayTracingPipelinesKHR");
            pvkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(logicalDevice, "vkGetRayTracingShaderGroupHandlesKHR");
        }

        else if (logicalDeviceExtension == VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
            pvkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(logicalDevice, "vkGetBufferDeviceAddressKHR");

        else
            Console::WriteLine("Given logical device extension " + logicalDeviceExtension + " has no activatable functions", MESSAGE_SEVERITY_WARNING);
    }
}

#pragma region VulkanExtensionFunctionDefinitions
VkDeviceAddress vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) 
{ 
#ifdef _DEBUG // gives warning C4297 (doesnt expect a throw), but can (presumably) be ignored because it's only for debug
    if (pvkGetBufferDeviceAddressKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    return pvkGetBufferDeviceAddressKHR(device, pInfo); 
}

VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
#ifdef _DEBUG
    if (pvkGetAccelerationStructureDeviceAddressKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    return pvkGetAccelerationStructureDeviceAddressKHR(device, pInfo);
}

VkResult vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) 
{ 
#ifdef _DEBUG
    if (pvkCreateRayTracingPipelinesKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    return pvkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); 
}

VkResult vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure)
{
#ifdef _DEBUG
    if (pvkCreateAccelerationStructureKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    return pvkCreateAccelerationStructureKHR(device, pCreateInfo, pAllocator, pAccelerationStructure);
}

VkResult vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData)
{
#ifdef _DEBUG
    if (pvkGetRayTracingShaderGroupHandlesKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    return pvkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

void vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo)
{
#ifdef _DEBUG
    if (pvkGetAccelerationStructureBuildSizesKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    pvkGetAccelerationStructureBuildSizesKHR(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

void vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator)
{
#ifdef _DEBUG
    if (pvkDestroyAccelerationStructureKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    pvkDestroyAccelerationStructureKHR(device, accelerationStructure, pAllocator);
}

void vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
{
#ifdef _DEBUG
    if (pvkCmdBuildAccelerationStructuresKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    pvkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

void vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth)
{
#ifdef _DEBUG
    if (pvkCmdTraceRaysKHR == nullptr)
        throw VulkanAPIError("Function \"" + (std::string)__FUNCTION__ + "\" was called, but is invalid.\nIts extension \"" + VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"", VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
#endif
    pvkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}
#pragma endregion VulkanExtensionFunctionDefinitions