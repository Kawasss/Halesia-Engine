#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <future>
#include "PhysicalDevice.h"

struct VulkanCreationObject;
typedef VulkanCreationObject TextureCreationObject;

class Image
{
public:
	void GenerateImages(const TextureCreationObject& creationObjects, std::vector<std::vector<char>>& textureData, bool useMipMaps = true);
	void GenerateEmptyImages(const TextureCreationObject& creationObjects, int width, int height, int amount);
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
	Cubemap(const TextureCreationObject& creationObjects, std::vector<std::string> filePath, bool useMipMaps = true);
	Cubemap(const TextureCreationObject& creationObjects, std::vector<std::vector<char>> filePath, bool useMipMaps = true);
};

class Texture : public Image
{
public:
	static Texture* placeholderAlbedo;
	static Texture* placeholderNormal;
	static Texture* placeholderMetallic;
	static Texture* placeholderRoughness;
	static Texture* placeholderAmbientOcclusion;
	static void GeneratePlaceholderTextures(const TextureCreationObject& creationObjects);
	static void DestroyPlaceholderTextures();

	Texture(const TextureCreationObject& creationObjects, std::string filePath, bool useMipMaps = true);
	Texture(const TextureCreationObject& creationObjects, std::vector<char> imageData, bool useMipMaps = true);
};