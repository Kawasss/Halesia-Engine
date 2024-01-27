#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <future>
#include "PhysicalDevice.h"

enum TextureFormat
{
	TEXTURE_FORMAT_SRGB = VK_FORMAT_R8G8B8A8_SRGB,
	TEXTURE_FORMAT_UNORM = VK_FORMAT_R8G8B8A8_UNORM,
};

class Image
{
public:
	void GenerateImages(std::vector<std::vector<char>>& textureData, bool useMipMaps = true, TextureFormat format = TEXTURE_FORMAT_SRGB);
	void GenerateEmptyImages(int width, int height, int amount);
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

	int				width = 0, height = 0;
	uint32_t		mipLevels = 1, layerCount = 0;
	VkDevice		logicalDevice;
	VkCommandPool	commandPool;
	VkQueue			queue;
	PhysicalDevice	physicalDevice;

	void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void CopyBufferToImage(VkBuffer buffer);
	void GenerateMipMaps(VkFormat imageFormat);

	static bool texturesHaveChanged;
};

class Cubemap : public Image
{
public:
	Cubemap(std::vector<std::string> filePath, bool useMipMaps = true);
	Cubemap(std::vector<std::vector<char>> filePath, bool useMipMaps = true);
};

class Texture : public Image
{
public:
	static Texture* placeholderAlbedo;
	static Texture* placeholderNormal;
	static Texture* placeholderMetallic;
	static Texture* placeholderRoughness;
	static Texture* placeholderAmbientOcclusion;
	static void GeneratePlaceholderTextures();
	static void DestroyPlaceholderTextures();

	Texture(std::string filePath, bool useMipMaps = true, TextureFormat format = TEXTURE_FORMAT_SRGB);
	Texture(std::vector<char> imageData, bool useMipMaps = true);
};