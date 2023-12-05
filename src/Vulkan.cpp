#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
#define NOMINMAX
#include <iostream>
#include <algorithm>

#include "renderer/Vulkan.h"
#include "renderer/Swapchain.h"
#include "Console.h"
#include "CreationObjects.h"

VkDebugUtilsMessengerEXT Vulkan::debugMessenger;
std::unordered_map<uint32_t, std::vector<VkCommandPool>> Vulkan::queueCommandPools;
std::unordered_map<VkDevice, std::mutex> Vulkan::logicalDeviceMutexes;
std::mutex Vulkan::graphicsQueueMutex;
std::mutex Vulkan::commandPoolMutex;
VkMemoryAllocateFlagsInfo* Vulkan::optionalMemoryAllocationFlags = nullptr;

VulkanAPIError::VulkanAPIError(std::string message, VkResult result, std::string functionName, std::string file, std::string line)
{
    std::string vulkanError = result == VK_SUCCESS ? "\n\n" : ":\n\n " + (std::string)string_VkResult(result) + " "; // result can be VK_SUCCESS for functions that dont use a vulkan functions, i.e. looking for a physical device but there are none that fit the bill
    std::string location = functionName == "" ? "" : "from " + functionName;
    location += line == "" ? "" : " at line " + line;
    location += file == "" ? "" : " in " + file;
    this->message = message + vulkanError + location;
}

void Vulkan::PopulateDefaultViewport(VkViewport& viewport, Swapchain* swapchain)
{
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)swapchain->extent.width;
    viewport.height = (float)swapchain->extent.height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
}

void Vulkan::PopulateDefaultScissors(VkRect2D& scissors, Swapchain* swapchain)
{
    scissors.offset = { 0, 0 };
    scissors.extent = swapchain->extent;
}

std::mutex& Vulkan::FetchLogicalDeviceMutex(VkDevice logicalDevice)
{
    if (logicalDeviceMutexes.count(logicalDevice) == 0)
        logicalDeviceMutexes[logicalDevice]; // should create a new mutex (?)
    return logicalDeviceMutexes[logicalDevice];
}

VkCommandPool Vulkan::FetchNewCommandPool(const VulkanCreationObject& creationObject)
{
    std::lock_guard<std::mutex> lockGuard(commandPoolMutex);
    VkCommandPool commandPool;
    if (queueCommandPools.count(creationObject.queueIndex) == 0)    // create a new vector if the queue index isn't registered yet
        queueCommandPools[creationObject.queueIndex] = {};

    if (queueCommandPools[creationObject.queueIndex].empty())       // if there are no idle command pools, create a new one
    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = creationObject.queueIndex;

        VkResult result = vkCreateCommandPool(creationObject.logicalDevice, &createInfo, nullptr, &commandPool);
        CheckVulkanResult("Failed to create a command pool for the storage buffer", result, vkCreateCommandPool);
    }
    else                                                            // if there are idle command pools, get the last one and remove that from the vector
    {
        commandPool = queueCommandPools[creationObject.queueIndex].back();
        queueCommandPools[creationObject.queueIndex].pop_back();
    }
    return commandPool;
}

void Vulkan::YieldCommandPool(uint32_t index, VkCommandPool commandPool)
{
    std::lock_guard<std::mutex> lockGuard(commandPoolMutex);
    if (queueCommandPools.count(index) == 0)
        throw VulkanAPIError("Failed to yield a command pool, no matching queue family index could be found", VK_SUCCESS, __FUNCTION__, __FILENAME__, __STRLINE__);
    queueCommandPools[index].push_back(commandPool);
}

void Vulkan::DestroyAllCommandPools(VkDevice logicalDevice)
{
    for (std::pair<uint32_t, std::vector<VkCommandPool>> commandPools : queueCommandPools)
        for (VkCommandPool commandPool : commandPools.second)
            vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
    queueCommandPools.clear();
}

VkDeviceAddress Vulkan::GetDeviceAddress(VkDevice logicalDevice, VkBuffer buffer)
{
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;

    return vkGetBufferDeviceAddress(logicalDevice, &addressInfo);
}

VkDeviceAddress Vulkan::GetDeviceAddress(VkDevice logicalDevice, VkAccelerationStructureKHR accelerationStructure)
{
    VkAccelerationStructureDeviceAddressInfoKHR BLASAddressInfo{};
    BLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    BLASAddressInfo.accelerationStructure = accelerationStructure;

    return vkGetAccelerationStructureDeviceAddressKHR(logicalDevice, &BLASAddressInfo);
}

VkShaderModule Vulkan::CreateShaderModule(VkDevice logicalDevice, const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &module);
    CheckVulkanResult("Failed to create a shader module", result, vkCreateShaderModule);

    return module;
}

VkPipelineShaderStageCreateInfo Vulkan::GetGenericShaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits shaderStageBit, const char* name)
{
    VkPipelineShaderStageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    createInfo.stage = shaderStageBit;
    createInfo.module = module;
    createInfo.pName = name;

    return createInfo;
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

    std::lock_guard<std::mutex> lockGuard1(Vulkan::graphicsQueueMutex); // dont question it, it works
    std::lock_guard<std::mutex> lockGuard(endCommandMutex);
    result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    CheckVulkanResult("Failed to submit the single time commands queue", result, vkQueueSubmit);
    
    result = vkQueueWaitIdle(queue);
    CheckVulkanResult("Failed to wait for the queue idle", result, vkQueueWaitIdle);

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
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.size = size;
    createInfo.usage = usage;

    VkResult result = vkCreateBuffer(logicalDevice, &createInfo, nullptr, &buffer);
    CheckVulkanResult("Failed to create a new buffer", result, vkCreateBuffer);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = optionalMemoryAllocationFlags;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = Vulkan::GetMemoryType(memoryRequirements.memoryTypeBits, properties, physicalDevice);
    
    result = vkAllocateMemory(logicalDevice, &allocateInfo, nullptr, &bufferMemory);
    CheckVulkanResult("Failed to allocate " + std::to_string(memoryRequirements.size) + " bytes of memory", result, vkAllocateMemory);

    /*#ifndef NDEBUG
        Console::WriteLine("Created a buffer with " + std::to_string(size) + " bytes");
    #endif*/

    vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

uint32_t Vulkan::GetMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, PhysicalDevice physicalDevice)
{
    VkPhysicalDeviceMemoryProperties physicalMemoryProperties = physicalDevice.MemoryProperties();

    for (uint32_t i = 0; i < physicalMemoryProperties.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (physicalMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    throw VulkanAPIError("Failed to get the memory type " + (std::string)string_VkMemoryPropertyFlags(properties) + " for the physical device " + (std::string)physicalDevice.Properties().deviceName, VK_SUCCESS, nameof(GetMemoryType), __FILENAME__, __STRLINE__);
}

void Vulkan::CreateImage(VkDevice logicalDevice, PhysicalDevice physicalDevice, uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateFlags flags, VkImage& image, VkDeviceMemory& memory)
{
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = mipLevels;
    createInfo.arrayLayers = arrayLayers;
    createInfo.format = format;
    createInfo.tiling = tiling;
    createInfo.usage = usage;
    createInfo.flags = flags;
    
    VkResult result = vkCreateImage(logicalDevice, &createInfo, nullptr, &image);
    CheckVulkanResult("Failed to create an image", result, vkCreateImage);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = GetMemoryType(memoryRequirements.memoryTypeBits, properties, physicalDevice);

    result = vkAllocateMemory(logicalDevice, &allocateInfo, nullptr, &memory);
    CheckVulkanResult("Failed to allocate memory for the image", result, vkAllocateMemory);

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
    createInfo.subresourceRange.levelCount = mipLevels;
    createInfo.subresourceRange.layerCount = layerCount;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.baseMipLevel = 0;

    VkImageView imageView;

    VkResult result = vkCreateImageView(logicalDevice, &createInfo, nullptr, &imageView);
    CheckVulkanResult("Failed to create an image view", result, vkCreateImageView);
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

    uint32_t width = window->GetWidth(), height = window->GetHeight();
    
    VkExtent2D extent{};
    extent.width = std::clamp(width, capabitilies.minImageExtent.width, capabitilies.maxImageExtent.width);
    extent.height = std::clamp(height, capabitilies.minImageExtent.height, capabitilies.maxImageExtent.width);
    
    return extent;
}

PhysicalDevice Vulkan::GetBestPhysicalDevice(std::vector<PhysicalDevice> devices, Surface surface)
{
    for (size_t i = 0; i < devices.size(); i++)
        if (IsDeviceCompatible(devices[i], surface))
        {
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
    appInfo.engineVersion = VK_VERSION_1_3;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pEngineName = "Halesia";  

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
    std::cout << "Enabled instance extensions:" << "\n";
    for (const char* extension : requiredInstanceExtensions)
        std::cout << "  " + (std::string)extension << "\n";
    std::cout << "\n";
#endif

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    CheckVulkanResult("Couldn't create a Vulkan instance", result, vkCreateInstance);

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

void Vulkan::CheckDeviceRequirements(bool indicesHasValue, bool extensionsSupported, bool swapChainIsCompatible, bool samplerAnisotropy, bool shaderUniformBufferArrayDynamicIndexing, std::set<std::string> unsupportedExtensions)
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

bool Vulkan::IsDeviceCompatible(PhysicalDevice device, Surface surface)
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

bool Vulkan::CheckInstanceExtensionSupport(std::vector<const char*> extensions)
{
    std::vector<VkExtensionProperties> allExtensions = GetInstanceExtensions();
    std::set<std::string> stringExtensions(extensions.begin(), extensions.end());
    for (const VkExtensionProperties& property : allExtensions)
        stringExtensions.erase(property.extensionName);

    return stringExtensions.empty();
}

bool Vulkan::CheckLogicalDeviceExtensionSupport(PhysicalDevice physicalDevice, const std::vector<const char*> extensions, std::set<std::string>& unsupportedExtensions)
{
    std::vector<VkExtensionProperties> allExtensions = GetLogicalDeviceExtensions(physicalDevice);
    unsupportedExtensions = std::set<std::string>(extensions.begin(), extensions.end());
    for (const VkExtensionProperties& property : allExtensions)
        unsupportedExtensions.erase(property.extensionName);

    return unsupportedExtensions.empty();
}

bool Vulkan::CheckValidationSupport()
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

std::vector<VkExtensionProperties> Vulkan::GetInstanceExtensions()
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    return extensions;
}

std::vector<VkExtensionProperties> Vulkan::GetLogicalDeviceExtensions(PhysicalDevice physicalDevice)
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice.Device(), nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice.Device(), nullptr, &extensionCount, extensions.data());

    return extensions;
}

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

std::string CreateFunctionNotActivatedError(std::string functionName, std::string extensionName)
{
    return "Function \"" + functionName + "\" was called, but is invalid.\nIts extension \"" + extensionName + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"";
}

#pragma region VulkanExtensionFunctionDefinitions
VkDeviceAddress vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) 
{ 
    #ifdef _DEBUG // gives warning C4297 (doesnt expect a throw), but can (presumably) be ignored because it's only for debug
    if (pvkGetBufferDeviceAddressKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    return pvkGetBufferDeviceAddressKHR(device, pInfo); 
}

VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
    #ifdef _DEBUG
    if (pvkGetAccelerationStructureDeviceAddressKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    return pvkGetAccelerationStructureDeviceAddressKHR(device, pInfo);
}

VkResult vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) 
{ 
    #ifdef _DEBUG
    if (pvkCreateRayTracingPipelinesKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    return pvkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); 
}

VkResult vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure)
{
    #ifdef _DEBUG
    if (pvkCreateAccelerationStructureKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    return pvkCreateAccelerationStructureKHR(device, pCreateInfo, pAllocator, pAccelerationStructure);
}

VkResult vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData)
{
    #ifdef _DEBUG
    if (pvkGetRayTracingShaderGroupHandlesKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    return pvkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

void vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo)
{
    #ifdef _DEBUG
    if (pvkGetAccelerationStructureBuildSizesKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    pvkGetAccelerationStructureBuildSizesKHR(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

void vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator)
{
    #ifdef _DEBUG
    if (pvkDestroyAccelerationStructureKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    pvkDestroyAccelerationStructureKHR(device, accelerationStructure, pAllocator);
}

void vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
{
    #ifdef _DEBUG
    if (pvkCmdBuildAccelerationStructuresKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    pvkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

void vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth)
{
    #ifdef _DEBUG
    if (pvkCmdTraceRaysKHR == nullptr)
        throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __STRLINE__);
    #endif
    pvkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}
#pragma endregion VulkanExtensionFunctionDefinitions