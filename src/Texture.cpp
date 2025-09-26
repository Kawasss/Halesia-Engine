#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

#include <vulkan/vulkan.h>

#include <ktx.h>
#include <ktxvulkan.h>

#include "renderer/Vulkan.h"
#include "renderer/physicalDevice.h"
#include "renderer/Texture.h"
#include "renderer/Buffer.h"
#include "renderer/GarbageManager.h"
#include "renderer/VulkanAPIError.h"

#include "core/Console.h"

#include "io/IO.h"

bool Image::texturesHaveChanged = false;
Texture* Texture::placeholderAlbedo = nullptr;
Texture* Texture::placeholderNormal = nullptr;
Texture* Texture::placeholderMetallic = nullptr;
Texture* Texture::placeholderRoughness = nullptr;
Texture* Texture::placeholderAmbientOcclusion = nullptr;

uint8_t* Color::GetData() const
{
	return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(this));
}

template<typename T, void(func)(T)>
struct GenericDeleter
{
	void operator()(void* pData) const
	{
		func(reinterpret_cast<T>(pData));
	}
};

using StbiDeleter = GenericDeleter<void*, free>;
using KtxTextureDeleter = GenericDeleter<ktxTexture2*, ktxTexture2_Destroy>;

std::vector<char> Image::Encode(const std::span<const char>& raw, int width, int height)
{
	int len = 0;
	unsigned char* unowned = stbi_write_png_to_mem(reinterpret_cast<const unsigned char*>(raw.data()), width * 4, width, height, 4, &len);
	std::unique_ptr<char, StbiDeleter> encoded(reinterpret_cast<char*>(unowned));

	if (len == 0 || unowned == nullptr)
		return std::vector<char>();

	return std::vector<char>(unowned, unowned + len);
}

std::vector<char> Image::Decode(const std::span<const char>& encoded, int& outWidth, int& outHeight, DecodeOptions options, float scale, int componentCount)
{
	stbi_set_flip_vertically_on_load(options == DecodeOptions::Flip ? 1 : 0);

	int comp = 0;
	stbi_uc* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()), static_cast<int>(encoded.size()), &outWidth, &outHeight, &comp, componentCount);
	std::unique_ptr<char, StbiDeleter> decoded(reinterpret_cast<char*>(data));

	if (outWidth == 0 || outHeight == 0 || data == nullptr)
		return std::vector<char>();

	int scaledWidth  = static_cast<int>(std::ceil(outWidth * scale));
	int scaledHeight = static_cast<int>(std::ceil(outHeight * scale));

	if (scaledWidth == outWidth && scaledHeight == outHeight)
		return std::vector<char>(data, data + outWidth * outHeight * componentCount);

	std::vector<char> resized(scaledWidth * scaledHeight * componentCount);

	stbir_resize_uint8_linear(data, outWidth, outHeight, outWidth * componentCount, reinterpret_cast<unsigned char*>(resized.data()), scaledWidth, scaledHeight, scaledWidth * componentCount, STBIR_RGBA);

	outWidth = scaledWidth;
	outHeight = scaledHeight;

	return std::vector<char>(resized.data(), resized.data() + outWidth * outHeight * componentCount);
}

bool Image::TexturesHaveChanged()
{
	bool ret = texturesHaveChanged;
	texturesHaveChanged = false;
	return ret;
}

void Image::GenerateImages(const std::vector<char>& textureData, bool useMipMaps, int amount, VkFormat format, TextureUseCase useCase)
{
	this->format = format;

	layerCount = static_cast<uint32_t>(amount);
	
	WritePixelsToBuffer(reinterpret_cast<const uint8_t*>(textureData.data()), useMipMaps, format, (VkImageLayout)useCase, 4);
}

void Image::GenerateEmptyImages(int width, int height, int amount)
{
	const Vulkan::Context ctx = Vulkan::GetContext();
	this->width = width;
	this->height = height;
	this->layerCount = static_cast<uint32_t>(amount);

	VkDeviceSize layerSize = width * height * 4;
	VkDeviceSize imageSize = layerSize * amount;

	VkImageCreateFlags flags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	image = Vulkan::CreateImage(width, height, mipLevels, layerCount, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, flags);

	VkImageViewType viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	imageView = Vulkan::CreateImageView(image.Get(), viewType, mipLevels, layerCount, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

	// this transition seems like a waste of resources
	TransitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	TransitionImageLayout((VkFormat)format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	this->texturesHaveChanged = true;
}

static bool FormatIsSRGB(VkFormat format)
{
	return format == VK_FORMAT_R8_SRGB || format == VK_FORMAT_R8G8_SRGB || format == VK_FORMAT_R8G8B8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_BC7_SRGB_BLOCK;
}

bool Image::FormatIsCompressed(VkFormat format)
{
	return format == VK_FORMAT_BC7_SRGB_BLOCK || format == VK_FORMAT_BC7_UNORM_BLOCK || format == VK_FORMAT_BC3_UNORM_BLOCK || format == VK_FORMAT_BC3_SRGB_BLOCK;
}

void Image::WritePixelsToBuffer(const uint8_t* pixels, bool useMipMaps, VkFormat format, VkImageLayout layout, int componentCount)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);

	if (useMipMaps)
		CalculateMipLevels();

	this->format = format;

	if (size == 0)
		size = width * height * componentCount;

	if (size == 0)
		throw std::runtime_error("Invalid image found: layer size is 0");

	win32::CriticalLockGuard lockGuard(Vulkan::graphicsQueueSection);

	ImmediateBuffer stagingBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// copy all of the different sides of the cubemap into a single buffer
	uint8_t* data = stagingBuffer.Map<uint8_t>();

	memcpy(data, pixels, static_cast<size_t>(size));

	stagingBuffer.Unmap();

	VkImageCreateFlags flags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	image = Vulkan::CreateImage(width, height, mipLevels, layerCount, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, flags);

	TransitionImageLayout(format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer.Get());

	stagingBuffer.~ImmediateBuffer();
	
	VkImageViewType viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	imageView = Vulkan::CreateImageView(image.Get(), viewType, mipLevels, layerCount, format, VK_IMAGE_ASPECT_COLOR_BIT);

	useMipMaps ? GenerateMipMaps(format) : TransitionImageLayout(format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);

	Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);

	this->texturesHaveChanged = true;
}

void Image::ChangeData(uint8_t* data, uint32_t size, VkFormat format)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	Buffer stagingBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	win32::CriticalLockGuard lockGuard(Vulkan::graphicsQueueSection);

	void* ptr = stagingBuffer.Map();
	memcpy(ptr, data, size);
	stagingBuffer.Unmap();

	TransitionImageLayout((VkFormat)format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer.Get());
}

std::vector<char> Image::GetImageData() const
{
	AwaitGeneration();

	const Vulkan::Context& ctx = Vulkan::GetContext();
	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);

	ImmediateBuffer copyBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkBufferImageCopy imageCopy{};
	imageCopy.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };
	imageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopy.imageSubresource.baseArrayLayer = 0;
	imageCopy.imageSubresource.mipLevel = 0;
	imageCopy.imageSubresource.layerCount = 1;

	VkImageSubresourceRange subresourceRange{};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = image.Get();
	memoryBarrier.subresourceRange = subresourceRange;
	
	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool); // messy

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
	vkCmdCopyImageToBuffer(commandBuffer, image.Get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copyBuffer.Get(), 1, &imageCopy);

	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

	Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, commandBuffer, commandPool);
	Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);

	char* ptr = copyBuffer.Map<char>();

	std::vector<char> ret;
	ret.resize(size / sizeof(char));
	memcpy(ret.data(), ptr, size);

	copyBuffer.Unmap();

	return ret;
}

void Image::AwaitGeneration() const
{
	if (generation.valid())
		generation.get();
}

bool Image::HasFinishedLoading() const
{
	if (generation.valid() && generation.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		generation.get(); // change the status of the image if it it done loading
	return !generation.valid();
}

void Image::CalculateMipLevels()
{
	assert(width != 0 && height != 0);
	mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

Cubemap::Cubemap(const std::string& filePath, bool useMipMaps)
{
	// this async can't use the capture list ( [&] ), because then filePath gets wiped clean (??)
	generation = std::async([=]()
		{
			std::expected<std::vector<char>, bool> fileData = IO::ReadFile(filePath);
			if (!fileData.has_value())
				return;

			this->layerCount = 6;

			std::vector<char> data = *fileData;
			this->GenerateImages(data, useMipMaps, 1, VK_FORMAT_R8G8B8A8_UNORM, TextureUseCase::TEXTURE_USE_CASE_READ_ONLY);
			this->CreateLayerViews();
		});
}

Cubemap::Cubemap(int width, int height)
{
	GenerateEmptyImages(width, height, 6);
	CreateLayerViews();
}

void Cubemap::CreateLayerViews()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	for (int i = 0; i < 6; i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = image.Get();
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.levelCount = mipLevels;
		createInfo.subresourceRange.layerCount = 1;
		createInfo.subresourceRange.baseArrayLayer = i;
		createInfo.subresourceRange.baseMipLevel = 0;

		VkResult result = vkCreateImageView(ctx.logicalDevice, &createInfo, nullptr, &layerViews[i]);
		CheckVulkanResult("Failed to create an image view", result);
	}
}

Cubemap::~Cubemap()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	for (VkImageView view : layerViews)
		vgm::Delete(view);
}

VkFormat Texture::GetVkFormatFromType(Type type)
{
	switch (type)
	{
	case Type::Albedo: return VK_FORMAT_BC7_SRGB_BLOCK;
	case Type::Normal: return VK_FORMAT_BC7_UNORM_BLOCK;
	case Type::Metallic: return VK_FORMAT_BC3_UNORM_BLOCK;
	case Type::Roughness: return VK_FORMAT_BC3_UNORM_BLOCK;
	case Type::AmbientOcclusion: return VK_FORMAT_BC3_UNORM_BLOCK;
	}
	return VK_FORMAT_R8G8B8A8_UNORM;
}

VkFormat Texture::GetUncompressedFormatFromFormat(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_BC7_SRGB_BLOCK: return VK_FORMAT_R8G8B8A8_SRGB;
	case VK_FORMAT_BC7_UNORM_BLOCK: return VK_FORMAT_R8G8B8A8_UNORM;
	case VK_FORMAT_BC3_UNORM_BLOCK: return VK_FORMAT_R8_UNORM;
	}
	return VK_FORMAT_R8G8B8A8_UNORM;
}

static int GetComponentCount(Texture::Type type)
{
	switch (type)
	{
	case Texture::Type::Normal:
	case Texture::Type::Albedo:
		return 4;
	case Texture::Type::AmbientOcclusion:
	case Texture::Type::Roughness:
	case Texture::Type::Metallic:
		return 1;
	}
	return 4;
}


Texture::Texture(const std::vector<char>& imageData, uint32_t width, uint32_t height, Type type, bool useMipMaps, TextureUseCase useCase)
{
	if (imageData.empty())
		throw std::runtime_error("Invalid texture size: imageData.empty()");

	this->width = width;
	this->height = height;
	this->layerCount = 1;

	//generation = std::async([](Texture* texture, std::vector<char> imageData, bool useMipMaps, TextureFormat format) 
		//{
			uint8_t* data = (uint8_t*)imageData.data();
			WritePixelsToBuffer(data, useMipMaps, GetVkFormatFromType(type), (VkImageLayout)useCase, GetComponentCount(type));
			texturesHaveChanged = true;
		//}, this, imageData, useMipMaps, format);
}

Texture::Texture(const Color& color, TextureUseCase useCase)
{
	width = 1, height = 1, layerCount = 1;

	generation = std::async([=]()
		{
			this->WritePixelsToBuffer({ color.GetData() }, false, VK_FORMAT_R8G8B8A8_UNORM, (VkImageLayout)useCase, 4);
		});
}

Texture::Texture(int width, int height)
{
	this->width  = width;
	this->height = height;
	this->layerCount = 1;

	GenerateEmptyImages(width, height, 1);
}

static ktx_transcode_fmt_e GetKtxFormat(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_BC7_UNORM_BLOCK:
	case VK_FORMAT_BC7_SRGB_BLOCK:
		return KTX_TF_BC7_M6_OPAQUE_ONLY;
	case VK_FORMAT_BC3_SRGB_BLOCK:
	case VK_FORMAT_BC3_UNORM_BLOCK:
		return KTX_TF_BC3;
	}
	return KTX_TF_BC7_M6_OPAQUE_ONLY;
}

static std::vector<char> GetCompressedData(const std::span<const char>& data, VkFormat format, VkFormat uncompressedFormat, int& width, int& height, int componentCount)
{
	ktx_error_code_e err = KTX_SUCCESS;

	ktxTextureCreateInfo createInfo{};
	createInfo.baseDepth = 1;
	createInfo.vkFormat = uncompressedFormat;
	createInfo.glInternalformat = 1;
	createInfo.baseWidth = width;
	createInfo.baseHeight = height;
	createInfo.numDimensions = 2;
	createInfo.numLayers = 1;
	createInfo.numLevels = 1;
	createInfo.numFaces = 1;
	createInfo.isArray = KTX_FALSE;
	createInfo.generateMipmaps = KTX_FALSE;

	std::unique_ptr<ktxTexture2, KtxTextureDeleter> pTexture;

	ktxTexture2* pRaw = nullptr;
	err = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &pRaw);
	if (err != KTX_SUCCESS || pRaw == nullptr)
	{
		Console::WriteLine("Failed to create a KTX texture", Console::Severity::Error);
		return {};
	}

	pTexture.reset(pRaw);

	err = ktxTexture_SetImageFromMemory(ktxTexture(pRaw), 0, 0, 0, reinterpret_cast<const uint8_t*>(data.data()), data.size());
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to set the image of a KTX texture from memory", Console::Severity::Error);
		return {};
	}

	err = ktxTexture2_CompressBasis(pRaw, 255);
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to compress a KTX texture", Console::Severity::Error);
		return {};
	}

	ktx_transcode_fmt_e fmt = GetKtxFormat(format);
	err = ktxTexture2_TranscodeBasis(pRaw, fmt, KTX_TF_HIGH_QUALITY);
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to transcode a KTX texture", Console::Severity::Error);
		return {};
	}

	ktx_size_t offset = 0;
	err = ktxTexture_GetImageOffset(ktxTexture(pRaw), 0, 0, 0, &offset);
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to get image offset of a KTX texture", Console::Severity::Error);
		return {};
	}
		
	const ktx_uint8_t* pData = ktxTexture_GetData(ktxTexture(pRaw));
	if (pData == nullptr)
	{
		Console::WriteLine("Failed to get the data of a KTX texture", Console::Severity::Error);
		return {};
	}
		
	pData += offset;
	ktx_size_t size = ktxTexture_GetImageSize(ktxTexture(pRaw), 0);
	if (size == 0)
	{
		Console::WriteLine("Failed to get a valid KTX texture image size", Console::Severity::Error);
		return {};
	}

	std::vector<char> ret(size);
	std::memcpy(ret.data(), pData, size);

	return ret;
}

Texture* Texture::LoadFromForeignFormat(const std::string_view& file, Type type, bool useMipMaps, TextureUseCase useCase)
{
	Console::WriteLine("Started loading file \"{}\"", Console::Severity::Debug, file);

	Texture* pTexture = new Texture();
	pTexture->layerCount = 1;

	int componentCount = GetComponentCount(type);

	std::expected<std::vector<char>, bool> read = IO::ReadFile(file, IO::ReadOptions::None);
	if (!read.has_value())
		return nullptr;

	std::vector<char> data = Decode(*read, pTexture->width, pTexture->height, DecodeOptions::Flip, 1.0f, componentCount);
	if (data.empty())
	{
		delete pTexture;
		return nullptr;
	}

	Console::WriteLine("Started compressing file \"{}\"", Console::Severity::Debug, file);

	VkFormat format = GetVkFormatFromType(type);
	std::vector<char> compressed = GetCompressedData(data, format, GetUncompressedFormatFromFormat(format), pTexture->width, pTexture->height, componentCount);
	if (compressed.empty())
		return nullptr;
		
	pTexture->size = compressed.size();
	pTexture->WritePixelsToBuffer(reinterpret_cast<const uint8_t*>(compressed.data()), useMipMaps, format, static_cast<VkImageLayout>(useCase), componentCount);
	
	Console::WriteLine("Finished loading file \"{}\"", Console::Severity::Debug, file);
	return pTexture;
}

Texture* Texture::LoadFromInternalFormat(const std::span<char>& data, bool useMipMaps, TextureUseCase useCase)
{
	return nullptr;
}

Texture* Texture::CreateEmpty(int width, int height)
{
	Texture* pTexture = new Texture();
	pTexture->layerCount = 1;

	pTexture->width = width;
	pTexture->height = height;

	pTexture->GenerateEmptyImages(width, height, 1);
	return pTexture;
}

void Texture::GeneratePlaceholderTextures()
{
	placeholderAlbedo = LoadFromForeignFormat("textures/placeholderAlbedo.png", Type::Albedo, false);
	placeholderNormal = LoadFromForeignFormat("textures/placeholderNormal.png", Type::Normal, false);
	placeholderMetallic = LoadFromForeignFormat("textures/black.png", Type::Metallic, false);
	placeholderRoughness = LoadFromForeignFormat("textures/black.png", Type::Roughness, false);
	placeholderAmbientOcclusion = LoadFromForeignFormat("textures/white.png", Type::AmbientOcclusion, false);

	placeholderAlbedo->AwaitGeneration();
	placeholderNormal->AwaitGeneration();
	placeholderMetallic->AwaitGeneration();
	placeholderRoughness->AwaitGeneration();
	placeholderAmbientOcclusion->AwaitGeneration();
}

void Texture::DestroyPlaceholderTextures()
{
	delete placeholderAlbedo;
	delete placeholderNormal;
	delete placeholderMetallic;
	delete placeholderRoughness;
	delete placeholderAmbientOcclusion;
}

void Image::TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer commandBuffer)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	bool isSingleTime = commandBuffer == VK_NULL_HANDLE;

	VkCommandPool commandPool = isSingleTime ? Vulkan::FetchNewCommandPool(ctx.graphicsIndex) : VK_NULL_HANDLE;
	if (isSingleTime)
		commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = oldLayout;
	memoryBarrier.newLayout = newLayout;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = image.Get();
	memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memoryBarrier.subresourceRange.baseMipLevel = 0;
	memoryBarrier.subresourceRange.levelCount = mipLevels;
	memoryBarrier.subresourceRange.baseArrayLayer = 0;
	memoryBarrier.subresourceRange.layerCount = layerCount;

	VkPipelineStageFlags sourceStage{};
	VkPipelineStageFlags destinationStage{};

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = 0;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else throw std::invalid_argument("Invalid layout transition");

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

	if (isSingleTime)
	{
		Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, commandBuffer, commandPool);
		Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);
	}
}

void Image::TransitionForShaderRead(VkCommandBuffer commandBuffer)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	bool useSingleTime = commandBuffer == VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	if (useSingleTime)
	{
		commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
		commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);
	}
		
	TransitionImageLayout((VkFormat)format, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

	if (useSingleTime)
	{
		Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, commandBuffer, commandPool);
		Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);
	}
}

void Image::TransitionForShaderWrite(VkCommandBuffer commandBuffer)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	bool useSingleTime = commandBuffer == VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	if (useSingleTime)
	{
		commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
		commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);
	}

	TransitionImageLayout((VkFormat)format, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, commandBuffer);

	if (useSingleTime)
	{
		Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, commandBuffer, commandPool);
		Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);
	}
}

void Image::CopyBufferToImage(VkBuffer buffer)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = layerCount;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };

	vkCmdCopyBufferToImage(commandBuffer, buffer, image.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, commandBuffer, commandPool);
	Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);
}

void Image::GenerateMipMaps(VkFormat imageFormat)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(ctx.physicalDevice.Device(), imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		throw std::runtime_error("Failed to find support for optimal tiling with the given format");

	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.image = image.Get();
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	memoryBarrier.subresourceRange.baseArrayLayer = 0;
	memoryBarrier.subresourceRange.layerCount = 1;
	memoryBarrier.subresourceRange.levelCount = 1;

	int32_t mipMapWidth = width, mipMapHeight = height;

	for (uint32_t i = 1; i < mipLevels; i++)
	{
		memoryBarrier.subresourceRange.baseMipLevel = i - 1;
		memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

		VkImageBlit blit{};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipMapWidth, mipMapHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = layerCount;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipMapWidth > 1 ? mipMapWidth / 2 : 1, mipMapHeight > 1 ? mipMapHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = layerCount;

		vkCmdBlitImage(commandBuffer, image.Get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		memoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

		if (mipMapWidth > 1) mipMapWidth /= 2;
		if (mipMapHeight > 1) mipMapHeight /= 2;
	}

	memoryBarrier.subresourceRange.baseMipLevel = mipLevels - 1;
	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

	Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, commandBuffer, commandPool);
	Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);
}

int Image::GetWidth() const
{
	return width;
}

int Image::GetHeight() const
{
	return height;
}

int Image::GetMipLevels() const
{
	return mipLevels;
}

void Image::Destroy()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	this->texturesHaveChanged = true;

	vgm::Delete(imageView);
	image.Destroy();
}