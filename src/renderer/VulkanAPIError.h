#pragma once
#include <vulkan/vulkan.h>
#include <source_location>
#include <exception>
#include <string>

class VulkanAPIError : public std::exception
{
public:
    VulkanAPIError(std::string message, VkResult result = VK_SUCCESS, std::source_location location = std::source_location::current());
    const char* what() const override { return message.c_str(); }

private:
    std::string message;
};

inline void CheckVulkanResult(std::string message, VkResult result = VK_SUCCESS, std::source_location location = std::source_location::current())
{
    if (result != VK_SUCCESS)
        throw VulkanAPIError(message, result, location);
}