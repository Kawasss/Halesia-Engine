#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include <future>
#include "../CreationObjects.h"

class Image
{
public:
	void GenerateImages(const TextureCreationObjects& creationObjects, std::vector<std::vector<char>>& textureData, bool useMipMaps);
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
	PhysicalDevice physicalDevice;

	void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void CopyBufferToImage(VkBuffer buffer);
	void GenerateMipMaps(VkFormat imageFormat);

	static bool texturesHaveChanged;
};

class Cubemap : public Image
{
public:
	Cubemap(const TextureCreationObjects& creationObjects, std::vector<std::string> filePath, bool useMipMaps);
	Cubemap(const TextureCreationObjects& creationObjects, std::vector<std::vector<char>> filePath, bool useMipMaps);
};

class Texture : public Image
{
public:
	static Texture* placeholderAlbedo;
	static Texture* placeholderNormal;
	static Texture* placeholderMetallic;
	static Texture* placeholderRoughness;
	static Texture* placeholderAmbientOcclusion;
	static void GeneratePlaceholderTextures(const TextureCreationObjects& creationObjects);
	static void DestroyPlaceholderTextures();

	Texture(const TextureCreationObjects& creationObjects, std::string filePath, bool useMipMaps);
	Texture(const TextureCreationObjects& creationObjects, std::vector<char> imageData, bool useMipMaps);
};