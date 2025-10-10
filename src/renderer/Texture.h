#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <future>
#include <array>
#include <span>

#include "PhysicalDevice.h"
#include "VideoMemoryManager.h"

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
	static std::vector<char> Decode(const std::span<const char>& encoded, int& outWidth, int& outHeight, DecodeOptions options, float scale, int componentCount); // used to decode foreign image formats

	void GenerateImages(const std::vector<char>& textureData, bool useMipMaps, int amount, VkFormat format, TextureUseCase useCase);
	//void GenerateImage(const char* data, bool useMipMaps, VkFormat format, TextureUseCase useCase);
	void GenerateCubemap(const char* data, TextureUseCase useCase);

	void GenerateEmptyImages(int width, int height, int amount);
	void ChangeData(uint8_t* data, uint32_t size, VkFormat format);
	void AwaitGeneration() const;
	bool HasFinishedLoading() const;
	void Destroy();

	int GetWidth() const;
	int GetHeight() const;
	int GetMipLevels() const;

	void TransitionForShaderWrite(VkCommandBuffer commandBuffer = VK_NULL_HANDLE);
	void TransitionForShaderRead(VkCommandBuffer commandBuffer = VK_NULL_HANDLE);

	std::vector<char> GetImageData() const;

	std::vector<char> GetAsInternalFormat() const;

	static bool TexturesHaveChanged();

	vvm::Image image;
	VkImageView imageView = VK_NULL_HANDLE;
	VkDeviceSize size = 0; // in bytes!

	~Image() { Destroy(); }

protected:
	mutable std::future<void> generation;

	int		 width = 0, height = 0;
	uint32_t mipLevels = 1, layerCount = 0;
	VkFormat format;

	void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer commandBuffer = VK_NULL_HANDLE);
	void CopyBufferToImage(VkBuffer buffer);
	void GenerateMipMaps(VkFormat imageFormat);
	void WritePixelsToBuffer(const uint8_t* pixels, bool useMipMaps, VkFormat format, VkImageLayout layout, int componentCount);
	void CalculateMipLevels();

	static bool texturesHaveChanged;

private:
	static bool FormatIsCompressed(VkFormat format);
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

class Texture : public Image // textures are only to be used for materials
{
public:
	enum class Type
	{
		Albedo,
		Normal,
		Metallic,
		Roughness,
		AmbientOcclusion,
	};

	static Texture* placeholderAlbedo;
	static Texture* placeholderNormal;
	static Texture* placeholderMetallic;
	static Texture* placeholderRoughness;
	static Texture* placeholderAmbientOcclusion;
	static void GeneratePlaceholderTextures();
	static void DestroyPlaceholderTextures();

	static Texture* LoadFromForeignFormat(const std::string_view& file, Type type, bool useMipMaps = true, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY);
	static Texture* LoadFromInternalFormat(const std::span<const char>& data, bool useMipMaps = true, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY);
	static Texture* CreateEmpty(int width, int height);

	Texture(int width, int height);
	Texture(const std::vector<char>& imageData, uint32_t width, uint32_t height, Type type, bool useMipMaps = true, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY); // uncompressed image !!
	Texture(const Color& color, TextureUseCase useCase = TEXTURE_USE_CASE_READ_ONLY); // solid color textures cannot use mip maps because theyre already 1x1

private:
	Texture() {}

	static VkFormat GetVkFormatFromType(Type type);
	static VkFormat GetUncompressedFormatFromFormat(VkFormat format);
};