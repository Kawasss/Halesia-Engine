#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <future>
#include <array>
#include <span>

#include "PhysicalDevice.h"
#include "VideoMemoryManager.h"

enum TextureFormat
{
	TEXTURE_FORMAT_SRGB = VK_FORMAT_R8G8B8A8_SRGB,
	TEXTURE_FORMAT_UNORM = VK_FORMAT_R8G8B8A8_UNORM,
};

enum TextureUseCase
{
	TEXTURE_USE_CASE_READ_ONLY = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	TEXTURE_USE_CASE_GENERAL = VK_IMAGE_LAYOUT_GENERAL,
};

struct Color
{
	explicit Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
	explicit Color(float r, float g, float b, float a = 1.0f) : r(uint8_t(r * 255)), g(uint8_t(g * 255)), b(uint8_t(b * 255)), a(uint8_t(a * 255)) {}
	uint8_t* GetData() const;

	uint8_t r, g, b, a;
};

class Image
{
public:
	enum class DecodeOptions
	{
		None = 0,
		Flip = 1,
	};

	static std::vector<char> Encode(const std::span<const char>& raw, int width, int height);
	static std::vector<char> Decode(const std::span<const char>& encoded, int& outWidth, int& outHeight, DecodeOptions options);

	void GenerateImages(const std::vector<char>& textureData, bool useMipMaps = true, int amount = 1, TextureFormat format = TEXTURE_FORMAT_SRGB, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY);
	void GenerateImage(const char* data, bool useMipMaps = true, TextureFormat format = TEXTURE_FORMAT_SRGB, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY);
	void GenerateCubemap(const char* data, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY);

	void GenerateEmptyImages(int width, int height, int amount);
	void ChangeData(uint8_t* data, uint32_t size, TextureFormat format);
	void AwaitGeneration() const;
	bool HasFinishedLoading() const;
	void Destroy();

	int GetWidth() const;
	int GetHeight() const;
	int GetMipLevels() const;

	void TransitionForShaderWrite(VkCommandBuffer commandBuffer = VK_NULL_HANDLE);
	void TransitionForShaderRead(VkCommandBuffer commandBuffer = VK_NULL_HANDLE);

	std::vector<char> GetImageData() const;

	static bool TexturesHaveChanged();

	vvm::Image image;
	VkImageView imageView = VK_NULL_HANDLE;
	VkDeviceSize size = 0; // in bytes!

	~Image() { Destroy(); }

protected:
	mutable std::future<void> generation;

	int				width = 0, height = 0;
	uint32_t		mipLevels = 1, layerCount = 0;
	TextureFormat   format;

	void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer commandBuffer = VK_NULL_HANDLE);
	void CopyBufferToImage(VkBuffer buffer);
	void GenerateMipMaps(VkFormat imageFormat);
	void WritePixelsToBuffer(uint8_t* pixels, bool useMipMaps, TextureFormat format, VkImageLayout layout);
	void CalculateMipLevels();

	static bool texturesHaveChanged;
};

class Cubemap : public Image
{
public:
	Cubemap(const std::string& filePath, bool useMipMaps = true);
	Cubemap(std::vector<std::vector<char>> filePath, bool useMipMaps = true);
	Cubemap(int width, int height);

	~Cubemap();

	std::array<VkImageView, 6> layerViews;

private:
	void CreateLayerViews();
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

	Texture(int width, int height);
	Texture(std::string filePath, bool useMipMaps = true, TextureFormat format = TEXTURE_FORMAT_SRGB, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY);
	Texture(std::vector<char> imageData, uint32_t width, uint32_t height, bool useMipMaps = true, TextureFormat format = TEXTURE_FORMAT_SRGB, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY); // uncompressed image !!
	Texture(const Color& color, TextureFormat format = TEXTURE_FORMAT_SRGB, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY); // solid color textures cannot use mip maps because theyre already 1x1
};