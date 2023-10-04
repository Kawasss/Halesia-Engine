#include <string>
#include <set>
#include "renderer/Vulkan.h"

PhysicalDevice::PhysicalDevice(VkPhysicalDevice physicalDevice)
{
    this->physicalDevice = physicalDevice;
}

QueueFamilyIndices PhysicalDevice::QueueFamilies(Surface surface)
{
    QueueFamilyIndices queueFamily;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, properties.data());

    for (int i = 0; i < properties.size(); i++)
    {
        if (properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            queueFamily.graphicsFamily = i;

        VkBool32 presentFamilySupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface.VkSurface(), &presentFamilySupport);

        if (presentFamilySupport)
            queueFamily.presentFamily = i;

        if (queueFamily.HasValue())
            break;
    }

    return queueFamily;
}

VkFormat PhysicalDevice::GetDepthFormat()
{
    return GetSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat PhysicalDevice::GetSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
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

VkPhysicalDeviceProperties PhysicalDevice::Properties()
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);
    return properties;
}

VkPhysicalDeviceFeatures PhysicalDevice::Features()
{
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    return features;
}

uint64_t PhysicalDevice::AdditionalRAM()
{
    return Vulkan::GetPhysicalDeviceMemoryProperties(physicalDevice).memoryHeaps[1].size;
}

uint64_t PhysicalDevice::VRAM()
{
	return Vulkan::GetPhysicalDeviceMemoryProperties(physicalDevice).memoryHeaps[0].size;
}

VkDevice PhysicalDevice::GetLogicalDevice(Surface surface)
{
    VkDevice device;
    QueueFamilyIndices indices = QueueFamilies(surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

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

    //ray tracing support
    VkPhysicalDeviceBufferDeviceAddressFeatures addressFeatures{};
    addressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    addressFeatures.bufferDeviceAddress = VK_TRUE;
    addressFeatures.bufferDeviceAddressCaptureReplay = VK_FALSE;
    addressFeatures.bufferDeviceAddressMultiDevice = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{};
    accelerationStructure.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructure.pNext = &addressFeatures;
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

    //check for bindless support
    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
    indexingFeatures.runtimeDescriptorArray = VK_TRUE;
    indexingFeatures.pNext = &imageFeatures;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &indexingFeatures;
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

    if (!indexingFeatures.descriptorBindingPartiallyBound || !indexingFeatures.runtimeDescriptorArray)
        throw std::runtime_error("Bindless textures aren't supported, the engine can't work without them");

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.enabledExtensionCount = requiredLogicalDeviceExtensions.size();
    createInfo.ppEnabledExtensionNames = requiredLogicalDeviceExtensions.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create the logical device for the physical device " + (std::string)Properties().deviceName);

#ifdef _DEBUG
    std::cout << "Enabled logical device extensions:" << std::endl;
    for (const char* extension : requiredLogicalDeviceExtensions)
        std::cout << "  " + (std::string)extension << std::endl;
#endif

    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    return device;
}

VkPhysicalDevice PhysicalDevice::Device()
{
    return physicalDevice;
}