#include <set>

#include "renderer/Vulkan.h"
#include "renderer/Surface.h"

PhysicalDevice::PhysicalDevice(VkPhysicalDevice physicalDevice)
{
    this->physicalDevice = physicalDevice;
}

QueueFamilyIndices PhysicalDevice::QueueFamilies(Surface& surface) const
{
    QueueFamilyIndices queueFamily;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, properties.data());

    for (uint32_t i = 0; i < properties.size(); i++)
    {
        if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queueFamily.graphicsFamily = i;
        if (properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            queueFamily.computeFamily = i;
        if (properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
            queueFamily.transferFamily = i;

        VkBool32 presentFamilySupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface.VkSurface(), &presentFamilySupport);

        if (presentFamilySupport && !queueFamily.presentFamily.has_value())
            queueFamily.presentFamily = i;

        if (queueFamily.HasValue())
            break;
    }

    return queueFamily;
}

VkFormat PhysicalDevice::GetDepthFormat() const
{
    return GetSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat PhysicalDevice::GetSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            return format;
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            return format;
    }
    throw std::runtime_error("Failed to find a supported format");
}

VkPhysicalDeviceProperties PhysicalDevice::Properties() const
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    return properties;
}

VkPhysicalDeviceFeatures PhysicalDevice::Features() const
{
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    return features;
}

VkPhysicalDeviceMemoryProperties PhysicalDevice::MemoryProperties() const
{
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    return properties;
}

uint64_t PhysicalDevice::VRAM() const
{
    return MemoryProperties().memoryHeaps[0].size;
}

uint64_t PhysicalDevice::AdditionalRAM() const
{
    return MemoryProperties().memoryHeaps[1].size;
}

VkDevice PhysicalDevice::GetLogicalDevice(Surface& surface)
{
    VkDevice device;
    QueueFamilyIndices indices = QueueFamilies(surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value(), indices.computeFamily.value(), indices.transferFamily.value() };

    float queuePriority = 1;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    rayQueryFeatures.rayQuery = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{};
    accelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructure.pNext = &rayQueryFeatures;
    accelerationStructure.accelerationStructure = VK_TRUE;
    accelerationStructure.accelerationStructureCaptureReplay = VK_FALSE;
    accelerationStructure.accelerationStructureIndirectBuild = VK_FALSE;
    accelerationStructure.accelerationStructureHostCommands = VK_FALSE;
    accelerationStructure.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingFeatures{};
    rayTracingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingFeatures.pNext = &accelerationStructure;
    rayTracingFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingFeatures.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
    rayTracingFeatures.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
    rayTracingFeatures.rayTracingPipelineTraceRaysIndirect = VK_FALSE;
    rayTracingFeatures.rayTraversalPrimitiveCulling = VK_FALSE;

    VkPhysicalDeviceRobustness2FeaturesEXT imageFeatures{}; // is needed for using empty textures (allows for easy async loading)
    imageFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;
    imageFeatures.pNext = &rayTracingFeatures;
    imageFeatures.nullDescriptor = VK_TRUE;

    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.pNext = &imageFeatures;
    vulkan11Features.storageBuffer16BitAccess = VK_TRUE;

    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.pNext = &vulkan11Features;
    vulkan12Features.timelineSemaphore = VK_TRUE;               // denoiser
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE; // bindless textures
    vulkan12Features.runtimeDescriptorArray = VK_TRUE;          // bindless textures
    vulkan12Features.bufferDeviceAddress = VK_TRUE;             // buffer addresses for ray tracing
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &vulkan12Features;
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &vulkan13Features;
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

    if (!vulkan12Features.descriptorBindingPartiallyBound || !vulkan12Features.runtimeDescriptorArray)
        throw std::runtime_error("Bindless textures aren't supported, the engine can't work without them");

    const std::vector<const char*>& extensions = Vulkan::GetDeviceExtensions();

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create the logical device for the physical device " + (std::string)Properties().deviceName);

    return device;
}

VkPhysicalDevice PhysicalDevice::Device() const
{
    return physicalDevice;
}