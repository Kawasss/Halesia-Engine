#ifdef NDEBUG
bool enableValidationLayers = false;
#else
bool enableValidationLayers = true;
#endif
#include <iostream>
#include <algorithm>
#include <cassert>
#include <sstream>

#include <vulkan/vk_enum_string_helper.h>

#include "core/Console.h"

#define VK_USE_PLATFORM_WIN32_KHR
#include "renderer/Vulkan.h"
#include "renderer/Surface.h"
#include "renderer/VideoMemoryManager.h"

VkMemoryAllocateFlagsInfo* Vulkan::optionalMemoryAllocationFlags = nullptr;

std::map<uint32_t, std::vector<VkCommandPool>> Vulkan::queueCommandPools;
std::map<VkDevice, std::mutex>                 Vulkan::logicalDeviceMutexes;

win32::CriticalSection Vulkan::graphicsQueueSection;
win32::CriticalSection commandPoolSection;

VkDeviceSize Vulkan::allocatedMemory = 0;

std::vector<const char*> Vulkan::requiredLogicalDeviceExtensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef _DEBUG
    VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME,
    "VK_NV_ray_tracing_validation",
#endif
};
std::vector<const char*> Vulkan::requiredInstanceExtensions =
{
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
};
std::vector<const char*> Vulkan::validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};

std::vector<VkDynamicState> Vulkan::dynamicStates =
{
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR,
};

Vulkan::Context Vulkan::context{};
std::string forcedGPU;

VulkanAPIError::VulkanAPIError(std::string message, VkResult result, std::string functionName, std::string file, int line)
{
    std::stringstream stream; // result can be VK_SUCCESS for functions that dont use a vulkan functions, i.e. looking for a physical device but there are none that fit the bill
    stream << message << "\n\n";
    if (result != VK_SUCCESS)
        stream << string_VkResult(result) << " ";
    if (!functionName.empty())
        stream << "from " << functionName;
    if (line != 0)
        stream << " at line " << line;
    if (!file.empty())
        stream << " in " << file;

#ifdef _DEBUG
    uint32_t count = 0;
    vkGetQueueCheckpointDataNV(Vulkan::GetContext().graphicsQueue, &count, nullptr);
    if (count > 0)
    {
        VkCheckpointDataNV base{};
        base.sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;

        std::vector<VkCheckpointDataNV> checkpoints(count, base);
        vkGetQueueCheckpointDataNV(Vulkan::GetContext().graphicsQueue, &count, checkpoints.data());

        stream << "\n\nCheckpoints:\n";
        for (VkCheckpointDataNV& data : checkpoints)
        {
            const char* msg = static_cast<const char*>(data.pCheckpointMarker);
            size_t msgLength = strnlen_s(msg, 128); // if the message is not a valid string (null terminator could not be found) then the marker is an integer value

            stream << string_VkPipelineStageFlagBits(data.stage) << ", ";
            if (msgLength >= 128)
                stream << reinterpret_cast<uint64_t>(data.pCheckpointMarker);
            else
                stream << msg;
            stream << "\n\n";
        }
    }
#endif
    this->message = stream.str();
}

void Vulkan::AllocateCommandBuffers(const VkCommandBufferAllocateInfo& allocationInfo, std::vector<CommandBuffer>& commandBuffers)
{
    VkCommandBuffer* pCommandBuffers = reinterpret_cast<VkCommandBuffer*>(commandBuffers.data()); // should be safe since the CommandBuffer class only contains the VkCommandBuffer

    VkResult result = vkAllocateCommandBuffers(context.logicalDevice, &allocationInfo, pCommandBuffers);
    CheckVulkanResult("Failed to allocate the command buffer", result, vkAllocateCommandBuffers);
}

VkPipelineDynamicStateCreateInfo Vulkan::GetDynamicStateCreateInfo()
{
    return GetDynamicStateCreateInfo(dynamicStates);
}

void Vulkan::RemoveDynamicState(VkDynamicState state)
{
    auto it = std::find(dynamicStates.begin(), dynamicStates.end(), state);
    if (it != dynamicStates.end())
        dynamicStates.erase(it);
}

void Vulkan::AddDynamicState(VkDynamicState state)
{
    dynamicStates.push_back(state);
}

void Vulkan::AddInstanceExtension(const char* name)
{
    requiredInstanceExtensions.push_back(name);
}

void Vulkan::AddDeviceExtenion(const char* name)
{
    requiredLogicalDeviceExtensions.push_back(name);
}

void Vulkan::AddValidationLayer(const char* name)
{
    validationLayers.push_back(name);
}

void Vulkan::ForcePhysicalDevice(const std::string& name)
{
    forcedGPU = name;
}

void Vulkan::DebugNameObject(uint64_t object, VkObjectType type, const char* name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectHandle = object;
    nameInfo.objectType = type;
    nameInfo.pObjectName = name;

    VkResult result = vkSetDebugUtilsObjectNameEXT(context.logicalDevice, &nameInfo);
    CheckVulkanResult("Failed to name an object", result, vkSetDebugUtilsObjectNameEXT);
}

void Vulkan::TransitionColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, VkAccessFlags src, VkAccessFlags dst, VkPipelineStageFlags srcPipe, VkPipelineStageFlags dstPipe)
{
    VkImageMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    memoryBarrier.oldLayout = oldLayout;
    memoryBarrier.newLayout = newLayout;
    memoryBarrier.srcAccessMask = src;
    memoryBarrier.dstAccessMask = dst;
    memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    memoryBarrier.image = image;
    memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    memoryBarrier.subresourceRange.baseMipLevel = 0;
    memoryBarrier.subresourceRange.levelCount = 1;
    memoryBarrier.subresourceRange.baseArrayLayer = 0;
    memoryBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(commandBuffer, srcPipe, dstPipe, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
}

void Vulkan::StartDebugLabel(VkCommandBuffer commandBuffer, const std::string& label)
{
    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = label.c_str();

    vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &labelInfo);
}

void Vulkan::InsertDebugLabel(VkCommandBuffer commandBuffer, const std::string& label)
{
    VkDebugUtilsLabelEXT labelInfo{};
    labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    labelInfo.pLabelName = label.c_str();

    vkCmdInsertDebugUtilsLabelEXT(commandBuffer, &labelInfo);
}

void Vulkan::DisableValidationLayers()
{
    enableValidationLayers = false;
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

bool Vulkan::Context::IsValid() const
{
    return !(instance == VK_NULL_HANDLE || logicalDevice == VK_NULL_HANDLE || physicalDevice.Device() == VK_NULL_HANDLE);
}

void Vulkan::DeleteSubmittedObjects()
{
    //for (auto iter = deletionQueue.begin(); iter < deletionQueue.end(); iter++)
    //    (*iter)();
    //deletionQueue.clear();
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

VkPipelineViewportStateCreateInfo Vulkan::GetDynamicViewportStateCreateInfo()
{
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

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
    win32::CriticalLockGuard lockGuard(commandPoolSection);

    std::vector<VkCommandPool>& commandPools = queueCommandPools[queueIndex];
    VkCommandPool commandPool = VK_NULL_HANDLE;

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
    win32::CriticalLockGuard lockGuard(commandPoolSection);

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

win32::CriticalSection endCommandCritSection;
void Vulkan::EndSingleTimeCommands(VkQueue queue, VkCommandBuffer commandBuffer, VkCommandPool commandPool)
{
    VkResult result = vkEndCommandBuffer(commandBuffer);
    CheckVulkanResult("Failed to end the single time command buffer", result, vkQueueSubmit);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    win32::CriticalLockGuard guard(endCommandCritSection);
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

vvm::Buffer Vulkan::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    VkBuffer buffer = VK_NULL_HANDLE;
    CreateBufferHandle(buffer, size, usage);

    return vvm::AllocateBuffer(buffer, properties, optionalMemoryAllocationFlags);
}

void Vulkan::Destroy()
{
    vvm::ForceDestroy();
}

uint32_t Vulkan::GetMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, PhysicalDevice physicalDevice)
{
    VkPhysicalDeviceMemoryProperties physicalMemoryProperties = physicalDevice.MemoryProperties();

    for (uint32_t i = 0; i < physicalMemoryProperties.memoryTypeCount; i++)
        if ((typeFilter & (1 << i)) && (physicalMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;

    CheckVulkanResult("Failed to get the memory type " + (std::string)string_VkMemoryPropertyFlags(properties) + " for the physical device " + (std::string)physicalDevice.Properties().deviceName, VK_ERROR_DEVICE_LOST, GetMemoryType);
}

vvm::Image Vulkan::CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t arrayLayers, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageCreateFlags flags, VkImageLayout initialLayout)
{
    VkImageCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.initialLayout = initialLayout;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = mipLevels;
    createInfo.arrayLayers = arrayLayers;
    createInfo.format = format;
    createInfo.tiling = tiling;
    createInfo.usage = usage;
    createInfo.flags = flags;
    
    VkImage image = VK_NULL_HANDLE;
    VkResult result = vkCreateImage(context.logicalDevice, &createInfo, nullptr, &image);
    CheckVulkanResult("Failed to create an image", result, vkCreateImage);

    return vvm::AllocateImage(image, properties);
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

VkPresentModeKHR Vulkan::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes, bool vsync)
{
    for (const VkPresentModeKHR presentMode : presentModes)
    {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR && !vsync)
            return presentMode;
        else if (presentMode == VK_PRESENT_MODE_FIFO_KHR && vsync)
            return presentMode;
    }
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
    PhysicalDevice curr;
    VkPhysicalDeviceProperties currProperties{};

    for (size_t i = 0; i < devices.size(); i++)
    {
        VkPhysicalDeviceProperties properties = devices[i].Properties();

        bool capable = IsDeviceCompatible(devices[i], surface);
        bool gpuTypeIsBetterThanCurr = (curr.IsValid() && currProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU); // prefer a discrete gpu over an integrated gpu

        if (properties.deviceName == forcedGPU)
        {
            if (!capable) 
                throw VulkanAPIError("Cannot force GPU to " + forcedGPU + ": it isn't compatible", VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
            return devices[i];
        }

        if (curr.IsValid() && !gpuTypeIsBetterThanCurr)
            continue;

        curr = devices[i];
        currProperties = curr.Properties();
    }
    if (!forcedGPU.empty())
        throw VulkanAPIError("Cannot find forced GPU \"" + forcedGPU + '"', VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);

    if (curr.IsValid())
        return curr;

    std::string message = "There is no compatible vulkan GPU for this engine present: iterated through " + std::to_string(devices.size()) + " physical devices: \n";
    for (PhysicalDevice physicalDevice : devices)
        message += (std::string)physicalDevice.Properties().deviceName + "\n";
    throw VulkanAPIError(message, VK_SUCCESS, __FUNCTION__, __FILENAME__, __LINE__);
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
    appInfo.applicationVersion = VK_VERSION_1_4;
    appInfo.engineVersion = VK_VERSION_1_4;
    appInfo.apiVersion = VK_API_VERSION_1_4;
    appInfo.pEngineName = "Halesia";  

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;   

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    if (enableValidationLayers)
    {
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
    std::string name = device.Properties().deviceName;

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

std::string CreateFunctionNotActivatedError(const std::string_view& functionName, const std::string_view& extensionName)
{
    std::stringstream stream;
    stream << "Function \"" << functionName << "\" was called, but is invalid.\nIts extension \"" << extensionName << "\" has not been activated";
    return stream.str();
}

#ifdef _DEBUG
#define DEBUG_ONLY(cont) cont
#else
#define DEBUG_ONLY(cont)
#endif

#define DEFINE_DEVICE_FUNCTION(function)   static PFN_##function p##function = reinterpret_cast<PFN_##function>(vkGetDeviceProcAddr(Vulkan::GetContext().logicalDevice, #function))
#define DEFINE_INSTANCE_FUNCTION(function) static PFN_##function p##function = reinterpret_cast<PFN_##function>(vkGetInstanceProcAddr(instance, #function))

#define CHECK_VALIDITY_DEBUG(ptr, ext) \
DEBUG_ONLY(                            \
if (ptr == nullptr)                    \
    throw VulkanAPIError(CreateFunctionNotActivatedError(__FUNCTION__, ext), VK_ERROR_EXTENSION_NOT_PRESENT, __FUNCTION__, __FILENAME__, __LINE__));

#pragma region VulkanExtensionFunctionDefinitions
VkResult vkGetSemaphoreWin32HandleKHR(VkDevice device, const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle)
{
    DEFINE_DEVICE_FUNCTION(vkGetSemaphoreWin32HandleKHR);
    CHECK_VALIDITY_DEBUG(pvkGetSemaphoreWin32HandleKHR, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
    return pvkGetSemaphoreWin32HandleKHR(device, pGetWin32HandleInfo, pHandle);
}

VkResult vkGetMemoryWin32HandleKHR(VkDevice device, const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo, HANDLE* pHandle)
{
    DEFINE_DEVICE_FUNCTION(vkGetMemoryWin32HandleKHR);
    CHECK_VALIDITY_DEBUG(pvkGetMemoryWin32HandleKHR, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME)
    return pvkGetMemoryWin32HandleKHR(device, pGetWin32HandleInfo, pHandle);
}

VkDeviceAddress vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) 
{ 
    DEFINE_DEVICE_FUNCTION(vkGetBufferDeviceAddressKHR);
    CHECK_VALIDITY_DEBUG(pvkGetBufferDeviceAddressKHR, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    return pvkGetBufferDeviceAddressKHR(device, pInfo); 
}

VkDeviceAddress vkGetAccelerationStructureDeviceAddressKHR(VkDevice device, const VkAccelerationStructureDeviceAddressInfoKHR* pInfo)
{
    DEFINE_DEVICE_FUNCTION(vkGetAccelerationStructureDeviceAddressKHR);
    CHECK_VALIDITY_DEBUG(pvkGetAccelerationStructureDeviceAddressKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    return pvkGetAccelerationStructureDeviceAddressKHR(device, pInfo);
}

VkResult vkCreateRayTracingPipelinesKHR(VkDevice device, VkDeferredOperationKHR deferredOperation, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkRayTracingPipelineCreateInfoKHR* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) 
{ 
    DEFINE_DEVICE_FUNCTION(vkCreateRayTracingPipelinesKHR);
    CHECK_VALIDITY_DEBUG(pvkCreateRayTracingPipelinesKHR, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
    return pvkCreateRayTracingPipelinesKHR(device, deferredOperation, pipelineCache, createInfoCount, pCreateInfos, pAllocator, pPipelines); 
}

VkResult vkCreateAccelerationStructureKHR(VkDevice device, const VkAccelerationStructureCreateInfoKHR* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkAccelerationStructureKHR* pAccelerationStructure)
{
    DEFINE_DEVICE_FUNCTION(vkCreateAccelerationStructureKHR);
    CHECK_VALIDITY_DEBUG(pvkCreateAccelerationStructureKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    return pvkCreateAccelerationStructureKHR(device, pCreateInfo, pAllocator, pAccelerationStructure);
}

VkResult vkGetRayTracingShaderGroupHandlesKHR(VkDevice device, VkPipeline pipeline, uint32_t firstGroup, uint32_t groupCount, size_t dataSize, void* pData)
{
    DEFINE_DEVICE_FUNCTION(vkGetRayTracingShaderGroupHandlesKHR);
    CHECK_VALIDITY_DEBUG(pvkGetRayTracingShaderGroupHandlesKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    return pvkGetRayTracingShaderGroupHandlesKHR(device, pipeline, firstGroup, groupCount, dataSize, pData);
}

void vkGetAccelerationStructureBuildSizesKHR(VkDevice device, VkAccelerationStructureBuildTypeKHR buildType, const VkAccelerationStructureBuildGeometryInfoKHR* pBuildInfo, const uint32_t* pMaxPrimitiveCounts, VkAccelerationStructureBuildSizesInfoKHR* pSizeInfo)
{
    DEFINE_DEVICE_FUNCTION(vkGetAccelerationStructureBuildSizesKHR);
    CHECK_VALIDITY_DEBUG(pvkGetAccelerationStructureBuildSizesKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    pvkGetAccelerationStructureBuildSizesKHR(device, buildType, pBuildInfo, pMaxPrimitiveCounts, pSizeInfo);
}

void vkDestroyAccelerationStructureKHR(VkDevice device, VkAccelerationStructureKHR accelerationStructure, const VkAllocationCallbacks* pAllocator)
{
    DEFINE_DEVICE_FUNCTION(vkDestroyAccelerationStructureKHR);
    CHECK_VALIDITY_DEBUG(pvkDestroyAccelerationStructureKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    pvkDestroyAccelerationStructureKHR(device, accelerationStructure, pAllocator);
}

void vkCmdBuildAccelerationStructuresKHR(VkCommandBuffer commandBuffer, uint32_t infoCount, const VkAccelerationStructureBuildGeometryInfoKHR* pInfos, const VkAccelerationStructureBuildRangeInfoKHR* const* ppBuildRangeInfos)
{
    DEFINE_DEVICE_FUNCTION(vkCmdBuildAccelerationStructuresKHR);
    CHECK_VALIDITY_DEBUG(pvkCmdBuildAccelerationStructuresKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    pvkCmdBuildAccelerationStructuresKHR(commandBuffer, infoCount, pInfos, ppBuildRangeInfos);
}

void vkCmdTraceRaysKHR(VkCommandBuffer commandBuffer, const VkStridedDeviceAddressRegionKHR* pRaygenShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pMissShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pHitShaderBindingTable, const VkStridedDeviceAddressRegionKHR* pCallableShaderBindingTable, uint32_t width, uint32_t height, uint32_t depth)
{
    DEFINE_DEVICE_FUNCTION(vkCmdTraceRaysKHR);
    CHECK_VALIDITY_DEBUG(pvkCmdTraceRaysKHR, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    pvkCmdTraceRaysKHR(commandBuffer, pRaygenShaderBindingTable, pMissShaderBindingTable, pHitShaderBindingTable, pCallableShaderBindingTable, width, height, depth);
}

void vkCmdBeginDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
    DEBUG_ONLY(
        DEFINE_DEVICE_FUNCTION(vkCmdBeginDebugUtilsLabelEXT);
        CHECK_VALIDITY_DEBUG(pvkCmdBeginDebugUtilsLabelEXT, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        pvkCmdBeginDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
    );
}

void vkCmdEndDebugUtilsLabelEXT(VkCommandBuffer commandBuffer)
{
    DEBUG_ONLY(
        DEFINE_DEVICE_FUNCTION(vkCmdEndDebugUtilsLabelEXT);
        CHECK_VALIDITY_DEBUG(pvkCmdEndDebugUtilsLabelEXT, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        pvkCmdEndDebugUtilsLabelEXT(commandBuffer);
    );
}

void vkCmdInsertDebugUtilsLabelEXT(VkCommandBuffer commandBuffer, const VkDebugUtilsLabelEXT* pLabelInfo)
{
    DEBUG_ONLY(
        DEFINE_DEVICE_FUNCTION(vkCmdInsertDebugUtilsLabelEXT);
        CHECK_VALIDITY_DEBUG(pvkCmdInsertDebugUtilsLabelEXT, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        pvkCmdInsertDebugUtilsLabelEXT(commandBuffer, pLabelInfo);
    );
}

VkResult vkSetDebugUtilsObjectNameEXT(VkDevice device, const VkDebugUtilsObjectNameInfoEXT* pNameInfo)
{
    DEBUG_ONLY(
        DEFINE_DEVICE_FUNCTION(vkSetDebugUtilsObjectNameEXT);
        CHECK_VALIDITY_DEBUG(pvkSetDebugUtilsObjectNameEXT, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        return pvkSetDebugUtilsObjectNameEXT(device, pNameInfo);
    );
    return VK_SUCCESS;
}

void vkCmdSetCheckpointNV(VkCommandBuffer commandBuffer, const void* pCheckpointMarker)
{
    DEFINE_DEVICE_FUNCTION(vkCmdSetCheckpointNV);
    CHECK_VALIDITY_DEBUG(pvkCmdSetCheckpointNV, VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
    pvkCmdSetCheckpointNV(commandBuffer, pCheckpointMarker);
}

void vkGetQueueCheckpointDataNV(VkQueue queue, uint32_t* pCheckPointDataCount, VkCheckpointDataNV* pCheckpointData)
{
    DEFINE_DEVICE_FUNCTION(vkGetQueueCheckpointDataNV);
    CHECK_VALIDITY_DEBUG(pvkGetQueueCheckpointDataNV, VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
    pvkGetQueueCheckpointDataNV(queue, pCheckPointDataCount, pCheckpointData);
}
#pragma endregion VulkanExtensionFunctionDefinitions