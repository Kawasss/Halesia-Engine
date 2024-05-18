#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif
#define NOMINMAX
#include <iostream>
#include <algorithm>

#include "core/Console.h"
#include "renderer/Vulkan.h"
#include "renderer/Surface.h"

#define ATTACH_FUNCTION(function) p##function = (PFN_##function##)vkGetDeviceProcAddr(logicalDevice, #function) // relies on the fact that a VkDevice named logicalDevice and the function pointer already exists with DEFINE_VULKAN_FUNCTION_POINTER
#define DEFINE_FUNCTION_POINTER(function) PFN_##function p##function = nullptr 

VkMemoryAllocateFlagsInfo* Vulkan::optionalMemoryAllocationFlags = nullptr;

std::unordered_map<uint32_t, std::vector<VkCommandPool>> Vulkan::queueCommandPools;
std::unordered_map<VkDevice, std::mutex>                 Vulkan::logicalDeviceMutexes;

std::deque<std::function<void()>> Vulkan::deletionQueue;

std::mutex Vulkan::graphicsQueueMutex;
std::mutex Vulkan::commandPoolMutex;

std::vector<const char*> Vulkan::requiredLogicalDeviceExtensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
};
std::vector<const char*> Vulkan::requiredInstanceExtensions =
{
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
};
std::vector<const char*> Vulkan::validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

Vulkan::Context Vulkan::context{};

VulkanAPIError::VulkanAPIError(std::string message, VkResult result, std::string functionName, std::string file, int line)
{
    std::string vulkanError = result == VK_SUCCESS ? "\n\n" : ":\n\n " + (std::string)string_VkResult(result) + " "; // result can be VK_SUCCESS for functions that dont use a vulkan functions, i.e. looking for a physical device but there are none that fit the bill
    std::string location = functionName == "" ? "" : "from " + functionName;
    location += line == 0 ? "" : " at line " + line;
    location += file == "" ? "" : " in " + file;

    this->message = message + vulkanError + location;
}

void Vulkan::InitializeContext(Context context)
{
    Vulkan::context = context;
}

const Vulkan::Context& Vulkan::GetContext()
{
    assert(context.IsValid() && "Invalid call to Vulkan::GetContext(): the requested context does not exist");
    return context;
}

bool Vulkan::Context::IsValid()
{
    return instance == VK_NULL_HANDLE || logicalDevice == VK_NULL_HANDLE || physicalDevice.Device() == VK_NULL_HANDLE;
}

void Vulkan::DeleteSubmittedObjects()
{
    for (auto iter = deletionQueue.begin(); iter < deletionQueue.end(); iter++)
        (*iter)();
    deletionQueue.clear();
}

void Vulkan::SubmitObjectForDeletion(std::function<void()>&& func)
{
    deletionQueue.push_back(func);
}

bool Vulkan::LogicalDeviceExtensionIsSupported(PhysicalDevice physicalDevice, const char* extension)
{
    std::set<std::string> supported;
    CheckLogicalDeviceExtensionSupport(physicalDevice, { extension }, supported);
    return supported.empty();
}

bool Vulkan::InstanceExtensionIsSupported(const char* extension)
{
    return CheckInstanceExtensionSupport({ extension });
}

VkQueryPool Vulkan::CreateQueryPool(VkQueryType type, uint32_t amount)
{
    VkQueryPool ret;
    VkQueryPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    createInfo.queryCount = amount;

    VkResult result = vkCreateQueryPool(context.logicalDevice, &createInfo, nullptr, &ret);
    CheckVulkanResult("Failed to create a query pool", result, vkCreateQueryPool);
    return ret;
}

std::vector<uint64_t> Vulkan::GetQueryPoolResults(VkQueryPool queryPool, uint32_t amount, uint32_t offset)
{
    std::vector<uint64_t> ret(amount);
    vkGetQueryPoolResults(context.logicalDevice, queryPool, offset, amount, amount * sizeof(uint64_t), ret.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    return ret;
}

VkPipelineDynamicStateCreateInfo Vulkan::GetDynamicStateCreateInfo(std::vector<VkDynamicState>& dynamicStates)
{
    return VkPipelineDynamicStateCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, (uint32_t)dynamicStates.size(), dynamicStates.data() };
}

VkPipelineViewportStateCreateInfo  Vulkan::GetDefaultViewportStateCreateInfo(VkViewport& viewport, VkRect2D& scissors, VkExtent2D extents)
{
    PopulateDefaultScissors(scissors, extents);
    PopulateDefaultViewport(viewport, extents);

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissors;

    return viewportState;
}

void Vulkan::PopulateDefaultViewport(VkViewport& viewport, VkExtent2D extents)
{
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)extents.width;
    viewport.height = (float)extents.height;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
}

void Vulkan::PopulateDefaultScissors(VkRect2D& scissors, VkExtent2D extents)
{
    scissors.offset = { 0, 0 };
    scissors.extent = extents;
}

std::mutex& Vulkan::FetchLogicalDeviceMutex(VkDevice logicalDevice)
{
    if (logicalDeviceMutexes.count(logicalDevice) == 0)
        logicalDeviceMutexes[logicalDevice]; // should create a new mutex (?)
    return logicalDeviceMutexes[logicalDevice];
}

VkCommandPool Vulkan::FetchNewCommandPool(uint32_t queueIndex)
{
    std::lock_guard<std::mutex> lockGuard(commandPoolMutex);
    std::vector<VkCommandPool>& commandPools = queueCommandPools[queueIndex];
    VkCommandPool commandPool;

    if (commandPools.empty()) // if there are no idle command pools, create a new one
    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = queueIndex;

        VkResult result = vkCreateCommandPool(context.logicalDevice, &createInfo, nullptr, &commandPool);
        CheckVulkanResult("Failed to create a command pool for the storage buffer", result, vkCreateCommandPool);
    }
    else // if there are idle command pools, get the last one and remove that from the vector
    {
        commandPool = commandPools.back();
        commandPools.pop_back();
    }
    return commandPool;
}

void Vulkan::YieldCommandPool(uint32_t index, VkCommandPool commandPool)
{
    std::lock_guard<std::mutex> lockGuard(commandPoolMutex);
    if (queueCommandPools.count(index) == 0)
        throw VulkanAPIError("Failed to yield a command pool, no matching queue family index could be found", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
    queueCommandPools[index].push_back(commandPool);
}

void Vulkan::DestroyAllCommandPools()
{
    for (const auto& [index, commandPools] : queueCommandPools)
        for (VkCommandPool commandPool : commandPools)
            vkDestroyCommandPool(context.logicalDevice, commandPool, nullptr);
    queueCommandPools.clear();
}

VkDeviceAddress Vulkan::GetDeviceAddress(VkBuffer buffer)
{
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer;

    return vkGetBufferDeviceAddress(context.logicalDevice, &addressInfo);
}

VkDeviceAddress Vulkan::GetDeviceAddress(VkAccelerationStructureKHR accelerationStructure)
{
    VkAccelerationStructureDeviceAddressInfoKHR BLASAddressInfo{};
    BLASAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    BLASAddressInfo.accelerationStructure = accelerationStructure;

    return vkGetAccelerationStructureDeviceAddressKHR(context.logicalDevice, &BLASAddressInfo);
}

HANDLE Vulkan::GetWin32MemoryHandle(VkDeviceMemory memory)
{
    HANDLE handle = (void*)0;
    VkMemoryGetWin32HandleInfoKHR handleInfo{};
    handleInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR;
    handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    handleInfo.memory = memory;

    VkResult result = vkGetMemoryWin32HandleKHR(context.logicalDevice, &handleInfo, &handle);
    CheckVulkanResult("Failed to get the win32 handle of given memory", result, vkGetMemoryWin32HandleKHR);
    return handle;
}

VkShaderModule Vulkan::CreateShaderModule(const std::vector<char>& code)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    VkResult result = vkCreateShaderModule(context.logicalDevice, &createInfo, nullptr, &module);
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

void Vulkan::CopyBuffer(VkCommandPool commandPool, VkQueue queue, VkBuffer sourceBuffer, VkBuffer destinationBuffer, VkDeviceSize size)
{
    VkCommandBuffer localCommandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;

    vkCmdCopyBuffer(localCommandBuffer, sourceBuffer, destinationBuffer, 1, &copyRegion);

    Vulkan::EndSingleTimeCommands(context.graphicsQueue, localCommandBuffer, commandPool);
}

std::mutex endCommandMutex;
void Vulkan::EndSingleTimeCommands(VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool)
{
    VkResult result = vkEndCommandBuffer(commandBuffer);
    CheckVulkanResult("Failed to end the single time command buffer", result, vkQueueSubmit);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    std::lock_guard<std::mutex> lockGuard(endCommandMutex);
    result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    CheckVulkanResult("Failed to submit the single time commands queue", result, vkQueueSubmit);
    
    result = vkQueueWaitIdle(queue);
    CheckVulkanResult("Failed to wait for the queue idle", result, vkQueueWaitIdle);

    vkFreeCommandBuffers(context.logicalDevice, commandPool, 1, &commandBuffer);
}

VkCommandBuffer Vulkan::BeginSingleTimeCommands(VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandPool = commandPool;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer localCommandBuffer;
    vkAllocateCommandBuffers(context.logicalDevice, &allocateInfo, &localCommandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(localCommandBuffer, &beginInfo);
    CheckVulkanResult("Failed to begin single time commands", result, vkBeginCommandBuffer);

    return localCommandBuffer;
}

void Vulkan::CreateBufferHandle(VkBuffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage, void* pNext)
{
    VkBufferCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.pNext = pNext;

    VkResult result = vkCreateBuffer(context.logicalDevice, &createInfo, nullptr, &buffer);
    CheckVulkanResult("Failed to create a new buffer", result, vkCreateBuffer);
}

void Vulkan::AllocateMemory(VkDeviceMemory& memory, VkMemoryRequirements& memoryRequirements, VkMemoryPropertyFlags properties, void* pNext)
{
    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = pNext;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = Vulkan::GetMemoryType(memoryRequirements.memoryTypeBits, properties, context.physicalDevice);

    VkResult result = vkAllocateMemory(context.logicalDevice, &allocateInfo, nullptr, &memory);
    CheckVulkanResult("Failed to allocate " + std::to_string(memoryRequirements.size) + " bytes of memory", result, vkAllocateMemory);
}

void Vulkan::CreateExternalBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    VkExternalMemoryBufferCreateInfo externBufferInfo{};
    externBufferInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
    externBufferInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

    CreateBufferHandle(buffer, size, usage, &externBufferInfo);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(context.logicalDevice, buffer, &memoryRequirements);

    VkExportMemoryAllocateInfo allocInfoExport{}; // dont know how to feel about win32 stuff being here
    allocInfoExport.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    allocInfoExport.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
    allocInfoExport.pNext = optionalMemoryAllocationFlags;

    AllocateMemory(bufferMemory, memoryRequirements, properties, &allocInfoExport);
    vkBindBufferMemory(context.logicalDevice, buffer, bufferMemory, 0);
}

void Vulkan::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
    CreateBufferHandle(buffer, size, usage);

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(context.logicalDevice, buffer, &memoryRequirements);

    AllocateMemory(bufferMemory, memoryRequirements, properties, optionalMemoryAllocationFlags);
    vkBindBufferMemory(context.logicalDevice, buffer, bufferMemory, 0);
}

void Vulkan::ReallocateBuffer(VkBuffer buffer, VkDeviceMemory& memory, VkDeviceSize size, VkDeviceSize oldSize, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) // this relies on the fact that the memory can be mapped
{
    VkDeviceMemory newMem = VK_NULL_HANDLE;
    VkBuffer newBuffer    = VK_NULL_HANDLE;

    CreateBuffer(size, usage, properties, newBuffer, newMem);

    void* oldMemData = nullptr, *newMemData = nullptr;
    VkResult result = vkMapMemory(context.logicalDevice, memory, 0, oldSize, 0, &oldMemData);
    CheckVulkanResult("Canot map the memory needed for reallocation", result, vkMapMemory);

    result = vkMapMemory(context.logicalDevice, newMem, 0, oldSize, 0, &newMemData);
    CheckVulkanResult("Canot map the memory needed for reallocation", result, vkMapMemory);

    memcpy(newMemData, oldMemData, oldSize);

    vkUnmapMemory(context.logicalDevice, memory);
    vkUnmapMemory(context.logicalDevice, newMem);
    vkFreeMemory(context.logicalDevice, memory, nullptr);
    vkDestroyBuffer(context.logicalDevice, buffer, nullptr);
    
    memory = newMem;
    buffer = newBuffer;
}

uint32_t Vulkan::GetMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, PhysicalDevice physicalDevice)
{
    VkPhysicalDeviceMemoryProperties physicalMemoryProperties = physicalDevice.MemoryProperties();

    for (uint32_t i = 0; i < physicalMemoryProperties.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (physicalMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    CheckVulkanResult("Failed to get the memory type " + (std::string)string_VkMemoryPropertyFlags(properties) + " for the physical device " + (std::string)physicalDevice.Properties().deviceName, VK_ERROR_DEVICE_LOST, GetMemoryType);
}

void Vulkan::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateFlags flags, VkImage& image, VkDeviceMemory& memory)
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
    
    VkResult result = vkCreateImage(context.logicalDevice, &createInfo, nullptr, &image);
    CheckVulkanResult("Failed to create an image", result, vkCreateImage);

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(context.logicalDevice, image, &memoryRequirements);

    AllocateMemory(memory, memoryRequirements, properties);
    vkBindImageMemory(context.logicalDevice, image, memory, 0);
}

bool Vulkan::HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkImageView Vulkan::CreateImageView(VkImage image, VkImageViewType viewType, uint32_t mipLevels, uint32_t layerCount, VkFormat format, VkImageAspectFlags aspectFlags)
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
    VkResult result = vkCreateImageView(context.logicalDevice, &createInfo, nullptr, &imageView);
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

VkExtent2D Vulkan::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabitilies, uint32_t width, uint32_t height)
{
    if (capabitilies.currentExtent.width != std::numeric_limits<uint32_t>::max())
        return capabitilies.currentExtent;
    
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
    throw VulkanAPIError(message, VK_SUCCESS, nameof(GetBestPhysicalDevice), __FILENAME__, __LINE__);
}

Vulkan::SwapChainSupportDetails Vulkan::QuerySwapChainSupport(PhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.Device(), surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.Device(), surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.Device(), surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.Device(), surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device.Device(), surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkFence Vulkan::CreateFence(VkFenceCreateFlags flags, void* pNext)
{
    VkFenceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.flags = flags;
    createInfo.pNext = pNext;

    VkFence ret = VK_NULL_HANDLE;
    VkResult result = vkCreateFence(context.logicalDevice, &createInfo, nullptr, &ret);
    CheckVulkanResult("Failed to create a signaled fence", result, vkCreateFence);

    return ret;
}

VkSemaphore Vulkan::CreateSemaphore(void* pNext)
{
    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = pNext;

    VkSemaphore ret = VK_NULL_HANDLE;
    VkResult result = vkCreateSemaphore(context.logicalDevice, &createInfo, nullptr, &ret);
    CheckVulkanResult("Failed to create a semaphore", result, vkCreateSemaphore);

    return ret;
}

VkInstance Vulkan::GenerateInstance()
{
    if (enableValidationLayers && !CheckValidationSupport())
        throw VulkanAPIError("Failed the enable the required validation layers", VK_SUCCESS, nameof(GenerateInstance), __FILENAME__, __LINE__);

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

        GetDebugMessengerCreateInfo(debugCreateInfo);
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

    return instance;
}

void Vulkan::GetDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
}

std::vector<PhysicalDevice> Vulkan::GetPhysicalDevices(VkInstance instance)
{
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
        throw VulkanAPIError("No Vulkan compatible GPUs could be found", VK_SUCCESS, nameof(GetPhysicalDevices), __FILENAME__, __LINE__);

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
        throw VulkanAPIError("No compatible queue family could be found", VK_SUCCESS, nameof(!indices.HasValue()), __FILENAME__, __LINE__);

    else if (!extensionsSupported)
    {
        std::string message = "Failed to find support for one or more logical device extensions:\n";
        for (std::string extension : unsupportedExtensions)
            message += '\n' + extension;
        throw VulkanAPIError(message, VK_SUCCESS, nameof(Vulkan::CheckLogicalDeviceExtensionSupport(device, requiredLogicalDeviceExtensions)), __FILENAME__, __LINE__);
    }

    else if (!swapChainIsCompatible)
        throw VulkanAPIError("No support for a swapchain could be found", VK_ERROR_FEATURE_NOT_PRESENT, nameof(Vulkan::QuerySwapChainSupport(device, surface)), __FILENAME__, __LINE__);

    else if (!samplerAnisotropy)
        throw VulkanAPIError("Critical feature is missing: VkPhysicalDeviceFeatures::samplerAnisotropy", VK_ERROR_FEATURE_NOT_PRESENT, nameof(device.Features().samplerAnisotropy), __FILENAME__, __LINE__);

    else if (!shaderUniformBufferArrayDynamicIndexing)
        throw VulkanAPIError("Critical feature is missing: VkPhysicalDeviceFeatures::shaderUniformBufferArrayDynamicIndexing", VK_ERROR_FEATURE_NOT_PRESENT, nameof(device.Features().shaderUniformBufferArrayDynamicIndexing), __FILENAME__, __LINE__);
}

bool Vulkan::IsDeviceCompatible(PhysicalDevice device, Surface surface)
{
    QueueFamilyIndices indices = device.QueueFamilies(surface);
    std::set<std::string> unsupportedExtensions;
    bool extensionsSupported = CheckLogicalDeviceExtensionSupport(device, requiredLogicalDeviceExtensions, unsupportedExtensions);

    bool swapChainIsCompatible = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails support = QuerySwapChainSupport(device, surface.VkSurface());
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
DEFINE_FUNCTION_POINTER(vkGetBufferDeviceAddressKHR);
DEFINE_FUNCTION_POINTER(vkCreateRayTracingPipelinesKHR);
DEFINE_FUNCTION_POINTER(vkGetAccelerationStructureBuildSizesKHR);
DEFINE_FUNCTION_POINTER(vkCreateAccelerationStructureKHR);
DEFINE_FUNCTION_POINTER(vkDestroyAccelerationStructureKHR);
DEFINE_FUNCTION_POINTER(vkGetAccelerationStructureDeviceAddressKHR);
DEFINE_FUNCTION_POINTER(vkCmdBuildAccelerationStructuresKHR);
DEFINE_FUNCTION_POINTER(vkGetRayTracingShaderGroupHandlesKHR);
DEFINE_FUNCTION_POINTER(vkCmdTraceRaysKHR);
DEFINE_FUNCTION_POINTER(vkGetMemoryWin32HandleKHR);
DEFINE_FUNCTION_POINTER(vkGetSemaphoreWin32HandleKHR);
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
            ATTACH_FUNCTION(vkCmdBuildAccelerationStructuresKHR);
            ATTACH_FUNCTION(vkCreateAccelerationStructureKHR);
            ATTACH_FUNCTION(vkDestroyAccelerationStructureKHR);
            ATTACH_FUNCTION(vkGetAccelerationStructureBuildSizesKHR);
            ATTACH_FUNCTION(vkGetAccelerationStructureDeviceAddressKHR);
        }

        else if (logicalDeviceExtension == VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME)
        {
            ATTACH_FUNCTION(vkCmdTraceRaysKHR);
            ATTACH_FUNCTION(vkCreateRayTracingPipelinesKHR);
            ATTACH_FUNCTION(vkGetRayTracingShaderGroupHandlesKHR);
        }

        else if (logicalDeviceExtension == VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME)
            ATTACH_FUNCTION(vkGetBufferDeviceAddressKHR);

        else if (logicalDeviceExtension == VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME)
        {
            ATTACH_FUNCTION(vkGetMemoryWin32HandleKHR);
        }

        else if (logicalDeviceExtension == VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME)
            ATTACH_FUNCTION(vkGetSemaphoreWin32HandleKHR);

        else
            Console::WriteLine("Given logical device extension " + logicalDeviceExtension + " has no activatable functions", MESSAGE_SEVERITY_WARNING);
    }
}

std::string CreateFunctionNotActivatedError(std::string functionName, std::string extensionName)
{
    return "Function \"" + functionName + "\" was called, but is invalid.\nIts extension \"" + extensionName + "\" has not been activated with \"ActivateLogicalDeviceExtensionFunctions\"";
}

#ifdef _DEBUG
#define DEBUG_ONLY(cont) cont
#else
#define DEBUG_ONLY(cont)
#endif

#define CHECK_VALIDITY_DEBUG(ptr, ext) \
DEBUG_ONLY(                            \
if (ptr == nullptr)                    \
    throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, ext), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __LINE__));

#pragma region VulkanExtensionFunctionDefinitions
VkResult vkGetSemaphoreWin32HandleKHR(VkDevice logicalDevice, const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle)
{
    CHECK_VALIDITY_DEBUG(pvkGetSemaphoreWin32HandleKHR, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    return pvkGetSemaphoreWin32HandleKHR(logicalDevice, pGetWin32HandleInfo, pHandle);
}

VkResult vkGetMemoryWin32HandleKHR(VkDevice device, const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle)
{
    CHECK_VALIDITY_DEBUG(pvkGetSemaphoreWin32HandleKHR, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME)
    return pvkGetMemoryWin32HandleKHR(device, pGetWin32HandleInfo, pHandle);
}

VkDeviceAddress vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) 
{ 
    CHECK_VALIDITY_DEBUG(pvkGetBufferDeviceAddressKHR, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    return pvkGetBufferDeviceAddressKHR(device, pInfo); 
}

VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
    CHECK_VALIDITY_DEBUG(pvkGetAccelerationStructureDeviceAddressKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    return pvkGetAccelerationStructureDeviceAddressKHR(device, pInfo);
}

VkResult vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) 
{ 
    CHECK_VALIDITY_DEBUG(pvkCreateRayTracingPipelinesKHR, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    return pvkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); 
}

VkResult vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure)
{
    CHECK_VALIDITY_DEBUG(pvkCreateAccelerationStructureKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    return pvkCreateAccelerationStructureKHR(device, pCreateInfo, pAllocator, pAccelerationStructure);
}

VkResult vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData)
{
    CHECK_VALIDITY_DEBUG(pvkGetRayTracingShaderGroupHandlesKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    return pvkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

void vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo)
{
    CHECK_VALIDITY_DEBUG(pvkGetAccelerationStructureBuildSizesKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    pvkGetAccelerationStructureBuildSizesKHR(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

void vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator)
{
    CHECK_VALIDITY_DEBUG(pvkDestroyAccelerationStructureKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    pvkDestroyAccelerationStructureKHR(device, accelerationStructure, pAllocator);
}

void vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
{
    CHECK_VALIDITY_DEBUG(pvkCmdBuildAccelerationStructuresKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    pvkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

void vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth)
{
    CHECK_VALIDITY_DEBUG(pvkCmdTraceRaysKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    pvkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}
#pragma endregion VulkanExtensionFunctionDefinitions