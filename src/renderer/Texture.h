#pragma once
#include <string>
#include <vulkan/vulkan.h>

class Texture
{
public:
	Texture(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice physicalDevice, std::string filePath, bool useMipMaps);
	void Destroy();
	
	int GetWidth();
	int GetHeight();
	int GetMipLevels();

	VkImage image;
	VkImageView imageView;
	VkDeviceMemory imageMemory;

private:
	void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void CopyBufferToImage(VkBuffer buffer);
	void GenerateMipMaps(VkFormat imageFormat);

	int width = 0, height = 0, mipLevels = 0;
	VkDevice logicalDevice;
	VkCommandPool commandPool;
	VkQueue queue;
	PhysicalDevice phyiscalDevice;
};