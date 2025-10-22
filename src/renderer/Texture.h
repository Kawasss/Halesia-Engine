 #pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <future>
#include <array>
#include <span>

#include "PhysicalDevice.h"
#include "VideoMemoryManager.h"

class CommandBuffer;

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
	enum Flags
	{
		None = 0,
		UseMipMaps = 1 << 0,
	};

	enum class DecodeOptions
	{
		None = 0,
		Flip = 1,
	};

	static std::vector<char> Decode(const std::span<const char>& encoded, uint32_t& outWidth, uint32_t& outHeight, DecodeOptions options, float scale, int componentCount); // used to decode foreign image formats

	//void GenerateImages(const std::vector<char>& textureData, bool useMipMaps, int amount, VkFormat format, TextureUseCase useCase);

	void Create(uint32_t width, uint32_t height, uint32_t layerCount, VkFormat format, uint32_t pixelSize, VkImageUsageFlags usage, Flags flags);
	void CreateWithCustomSize(uint32_t width, uint32_t height, VkDeviceSize size, uint32_t layerCount, VkFormat format, VkImageUsageFlags usage, Flags flags);

	void UploadData(const std::span<const char>& data);

	void TransitionTo(VkImageLayout layout, CommandBuffer cmdBuffer);

	void AwaitGeneration() const;
	bool HasFinishedLoading() const;
	void Destroy();

	int GetWidth() const;
	int GetHeight() const;
	int GetMipLevels() const;

	std::vector<char> GetImageData() const;
	std::vector<char> GetAsInternalFormat() const;

	vvm::Image image;
	VkImageView imageView = VK_NULL_HANDLE;
	VkDeviceSize size = 0; // in bytes!

	~Image() { Destroy(); }

protected:
	mutable std::future<void> generation;

	uint32_t width = 0, height = 0;
	uint32_t mipLevels = 1, layerCount = 1;
	VkFormat format = VK_FORMAT_MAX_ENUM;
	VkImageUsageFlags usage = 0;
	VkImageLayout currLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	void CopyBufferToImage(VkBuffer buffer);
	void GenerateMipMaps();
	//void WritePixelsToBuffer(const uint8_t* pixels, bool useMipMaps, VkFormat format, VkImageLayout layout, int componentCount);
	void CalculateMipLevels();
	void SetAllAttributes(uint32_t width, uint32_t height, VkDeviceSize size, uint32_t layerCount, VkFormat format, VkImageUsageFlags usage);
	void CreateImageAndView();

private:
	static bool FormatIsCompressed(VkFormat format);
};

class Cubemap : public Image
{
public:
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

private:
	Texture() = default;

	static VkFormat GetVkFormatFromType(Type type);
	static VkFormat GetUncompressedFormatFromFormat(VkFormat format);
};