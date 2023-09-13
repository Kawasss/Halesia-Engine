#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include <future>

class Image
{
public:
	void GenerateImages(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice physicalDevice, std::vector<std::string> filePath, bool useMipMaps);
	void AwaitGeneration();
	bool HasFinishedLoading();
	void Destroy();

	int GetWidth();
	int GetHeight();
	int GetMipLevels();

	static bool TexturesHaveChanged();

	VkImage image;
	VkImageView imageView = VK_NULL_HANDLE;
	VkDeviceMemory imageMemory;

protected:
	std::future<void> generation;
	int width = 0, height = 0;
	uint32_t mipLevels = 1, layerCount = 0;
	VkDevice logicalDevice;
	VkCommandPool commandPool;
	VkQueue queue;
	PhysicalDevice phyiscalDevice;

	void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void CopyBufferToImage(VkBuffer buffer);
	void GenerateMipMaps(VkFormat imageFormat);

	static bool texturesHaveChanged;
};

class Cubemap : public Image
{
public:
	Cubemap(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice physicalDevice, std::vector<std::string> filePath, bool useMipMaps);
};

class Texture : public Image
{
public:
	static Texture* placeholderAlbedo;
	static Texture* placeholderNormal;
	static Texture* placeholderMetallic;
	static Texture* placeholderRoughness;
	static Texture* placeholderAmbientOcclusion;
	static void GeneratePlaceholderTextures(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice phyiscalDevice);
	static void DestroyPlaceholderTextures();

	Texture(VkDevice logicalDevice, VkQueue queue, VkCommandPool commandPool, PhysicalDevice physicalDevice, std::string filePath, bool useMipMaps);
};