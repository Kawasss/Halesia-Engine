#pragma once
#include <string>
#include <vulkan/vulkan.h>

class Image
{
public:
	void GenerateImages(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice physicalDevice, std::vector<std::string>& filePath, bool useMipMaps);
	void Destroy();

	int GetWidth();
	int GetHeight();
	int GetMipLevels();

	VkImage image;
	VkImageView imageView;
	VkDeviceMemory imageMemory;

private:
	

protected:
	int width = 0, height = 0;
	uint32_t mipLevels = 1, layerCount = 0;
	VkDevice logicalDevice;
	VkCommandPool commandPool;
	VkQueue queue;
	PhysicalDevice phyiscalDevice;

	void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void CopyBufferToImage(VkBuffer buffer);
	void GenerateMipMaps(VkFormat imageFormat);
};

class Cubemap : public Image//merge with texture into template + inheritance class
{
public:
	Cubemap(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice physicalDevice, std::vector<std::string>& filePath, bool useMipMaps);
};

class Texture : public Image
{
public:
	Texture(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice physicalDevice, std::string filePath, bool useMipMaps);
};