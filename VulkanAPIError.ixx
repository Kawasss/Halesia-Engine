export module Renderer.VulkanAPIError;

import std;

import <vulkan/vulkan.h>;

export class VulkanAPIError : public std::exception
{
public:
    VulkanAPIError(std::string message, VkResult result = VK_SUCCESS, std::source_location location = std::source_location::current());
    const char* what() const override { return message.c_str(); }

private:
    std::string message;
};

export void CheckVulkanResult(std::string message, VkResult result = VK_SUCCESS, std::source_location location = std::source_location::current())
{
    if (result != VK_SUCCESS)
        throw VulkanAPIError(message, result, location);
}