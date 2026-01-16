#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb/stb_image_resize2.h"

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>

#include <ktx.h>
#include <ktxvulkan.h>

#include "renderer/Vulkan.h"
#include "renderer/physicalDevice.h"
#include "renderer/Texture.h"
#include "renderer/Buffer.h"
#include "renderer/GarbageManager.h"
#include "renderer/VulkanAPIError.h"

#include "core/Console.h"

import std;

import Renderer.ImageTransitioner;

import IO;

Texture* Texture::placeholderAlbedo = nullptr;
Texture* Texture::placeholderNormal = nullptr;
Texture* Texture::placeholderMetallic = nullptr;
Texture* Texture::placeholderRoughness = nullptr;
Texture* Texture::placeholderAmbientOcclusion = nullptr;

constexpr VkImageUsageFlags TEXTURE_USAGE = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

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

std::vector<char> Image::Decode(const std::span<const char>& encoded, uint32_t& outWidth, uint32_t& outHeight, DecodeOptions options, float scale, int componentCount)
{
	stbi_set_flip_vertically_on_load(options == DecodeOptions::Flip ? 1 : 0);

	int comp = 0, width = 0, height = 0;
	stbi_uc* data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()), static_cast<int>(encoded.size()), &width, &height, &comp, componentCount);
	std::unique_ptr<char, StbiDeleter> decoded(reinterpret_cast<char*>(data));

	if (width == 0 || height == 0 || data == nullptr)
		return std::vector<char>();

	outWidth  = static_cast<uint32_t>(width);
	outHeight = static_cast<uint32_t>(height);

	int scaledWidth  = static_cast<int>(std::ceil(width * scale));
	int scaledHeight = static_cast<int>(std::ceil(height * scale));

	if (scaledWidth == width && scaledHeight == height)
		return std::vector<char>(data, data + width * height * componentCount);

	std::vector<char> resized(scaledWidth * scaledHeight * componentCount);

	stbir_resize_uint8_linear(data, outWidth, outHeight, outWidth * componentCount, reinterpret_cast<unsigned char*>(resized.data()), scaledWidth, scaledHeight, scaledWidth * componentCount, STBIR_RGBA);

	outWidth = scaledWidth;
	outHeight = scaledHeight;

	return std::vector<char>(resized.data(), resized.data() + outWidth * outHeight * componentCount);
}

void Image::Create(uint32_t width, uint32_t height, uint32_t layerCount, VkFormat format, uint32_t pixelSize, VkImageUsageFlags usage, Flags flags)
{
	SetAllAttributes(width, height, pixelSize * width * height * layerCount, layerCount, format, usage);

	if (flags & UseMipMaps)
		CalculateMipLevels();

	VkImageCreateFlags createFlags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	image = Vulkan::CreateImage(width, height, mipLevels, layerCount, format, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, createFlags);

	VkImageViewType viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	view = Vulkan::CreateImageView(image.Get(), viewType, mipLevels, layerCount, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Image::CreateWithCustomSize(uint32_t width, uint32_t height, VkDeviceSize size, uint32_t layerCount, VkFormat format, VkImageUsageFlags usage, Flags flags)
{
	SetAllAttributes(width, height, size, layerCount, format, usage);

	if (flags & UseMipMaps)
		CalculateMipLevels();

	CreateImageAndView();
}

void Image::InheritFrom(Image& other)
{
	Destroy();

	std::swap(image, other.image);
	std::swap(view, other.view);

	SetAllAttributes(other.width, other.height, other.size, other.layerCount, other.format, other.usage);
}

void Image::SetAllAttributes(uint32_t width, uint32_t height, VkDeviceSize size, uint32_t layerCount, VkFormat format, VkImageUsageFlags usage)
{
	this->width = width;
	this->height = height;
	this->size = size;
	this->layerCount = layerCount;
	this->format = format;
	this->usage = usage;
}

void Image::ResetAllAttributes()
{
	width = 0;
	height = 0;
	size = 0;
	layerCount = 1;
	mipLevels = 1;
	format = VK_FORMAT_MAX_ENUM;
	usage = 0;
	currLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void Image::CreateImageAndView()
{
	VkImageCreateFlags createFlags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	image = Vulkan::CreateImage(width, height, mipLevels, layerCount, format, VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, createFlags);

	VkImageViewType viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	view = Vulkan::CreateImageView(image.Get(), viewType, mipLevels, layerCount, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Image::UploadData(const std::span<const char>& data)
{
	assert(width != 0 && height != 0 && layerCount != 0 && mipLevels != 0 && usage != 0 && format != VK_FORMAT_MAX_ENUM && size != 0);
	assert(usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT && currLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	ImmediateBuffer stagingBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	char* pData = stagingBuffer.Map<char>();

	std::memcpy(pData, data.data(), data.size());

	stagingBuffer.Unmap();

	CopyBufferToImage(stagingBuffer.Get());

	if (mipLevels != 1)
		GenerateMipMaps();
}

static bool FormatIsSRGB(VkFormat format)
{
	return format == VK_FORMAT_R8_SRGB || format == VK_FORMAT_R8G8_SRGB || format == VK_FORMAT_R8G8B8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB || format == VK_FORMAT_BC7_SRGB_BLOCK;
}

bool Image::FormatIsCompressed(VkFormat format)
{
	return format == VK_FORMAT_BC7_SRGB_BLOCK || format == VK_FORMAT_BC7_UNORM_BLOCK || format == VK_FORMAT_BC3_UNORM_BLOCK || format == VK_FORMAT_BC3_SRGB_BLOCK;
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

std::vector<char> Image::GetAsInternalFormat() const
{
	std::vector<char> raw = GetImageData();

	ktx_error_code_e err = KTX_SUCCESS;

	ktxTextureCreateInfo createInfo{};
	createInfo.baseDepth = 1;
	createInfo.vkFormat = format;
	createInfo.glInternalformat = 1;
	createInfo.baseWidth = width;
	createInfo.baseHeight = height;
	createInfo.numDimensions = 2;
	createInfo.numLayers = 1;
	createInfo.numLevels = 1;
	createInfo.numFaces = layerCount;
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

	err = ktxTexture_SetImageFromMemory(ktxTexture(pRaw), 0, 0, 0, reinterpret_cast<const uint8_t*>(raw.data()), raw.size());
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to set the image of a KTX texture from memory", Console::Severity::Error);
		return {};
	}

	uint8_t* pImageFormat = nullptr;
	ktx_size_t size = 0;

	err = ktxTexture_WriteToMemory(ktxTexture(pRaw), &pImageFormat, &size);
	if (err != KTX_SUCCESS || pImageFormat == nullptr)
	{
		return {};
	}

	char* asChar = reinterpret_cast<char*>(pImageFormat);
	return std::vector<char>(asChar, asChar + size);
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

Cubemap::Cubemap(int width, int height)
{
	Create(width, height, 6, VK_FORMAT_R8G8B8A8_SRGB, sizeof(uint32_t), VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, None);
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

static std::vector<char> GetCompressedData(const std::span<const char>& data, VkFormat format, VkFormat uncompressedFormat, uint32_t& width, uint32_t& height, int componentCount)
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

	ktxTexture2* pRaw = nullptr;
	err = ktxTexture2_Create(&createInfo, KTX_TEXTURE_CREATE_ALLOC_STORAGE, &pRaw);
	if (err != KTX_SUCCESS || pRaw == nullptr)
	{
		Console::WriteLine("Failed to create a KTX texture ({})", Console::Severity::Error, ktxErrorString(err));
		return {};
	}
	std::unique_ptr<ktxTexture2, KtxTextureDeleter> pTexture(pRaw);

	err = ktxTexture_SetImageFromMemory(ktxTexture(pRaw), 0, 0, 0, reinterpret_cast<const uint8_t*>(data.data()), data.size());
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to set the image of a KTX texture from memory ({})", Console::Severity::Error, ktxErrorString(err));
		return {};
	}

	err = ktxTexture2_CompressBasis(pRaw, 0);
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to compress a KTX texture ({})", Console::Severity::Error, ktxErrorString(err));
		return {};
	}

	ktx_transcode_fmt_e fmt = GetKtxFormat(format);
	err = ktxTexture2_TranscodeBasis(pRaw, fmt, KTX_TF_TRANSCODE_ALPHA_DATA_TO_OPAQUE_FORMATS);
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to transcode a KTX texture ({})", Console::Severity::Error, ktxErrorString(err));
		return {};
	}

	ktx_size_t offset = 0;
	err = ktxTexture_GetImageOffset(ktxTexture(pRaw), 0, 0, 0, &offset);
	if (err != KTX_SUCCESS)
	{
		Console::WriteLine("Failed to get image offset of a KTX texture ({})", Console::Severity::Error, ktxErrorString(err));
		return {};
	}
		
	const ktx_uint8_t* pData = ktxTexture_GetData(ktxTexture(pRaw));
	if (pData == nullptr)
	{
		Console::WriteLine("Failed to get the data of a KTX texture ({})", Console::Severity::Error, ktxErrorString(err));
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

	std::unique_ptr<Texture> pTexture(new Texture());
	pTexture->layerCount = 1;

	int componentCount = GetComponentCount(type);
	uint32_t width = 0, height = 0;

	std::expected<std::vector<char>, bool> read = IO::ReadFile(file, IO::ReadOptions::None);
	if (!read.has_value())
		return nullptr;

	std::vector<char> data = Decode(*read, width, height, DecodeOptions::Flip, 1.0f, componentCount);
	if (data.empty())
		return nullptr;

	Console::WriteLine("Started compressing file \"{}\"", Console::Severity::Debug, file);

	VkFormat format = GetVkFormatFromType(type);
	std::vector<char> compressed = GetCompressedData(data, format, GetUncompressedFormatFromFormat(format), width, height, componentCount);
	if (compressed.empty())
		return nullptr;
		
	pTexture->CreateWithCustomSize(width, height, compressed.size(), 1, format, TEXTURE_USAGE, None); // hard-coded no mipmaps for now

	pTexture->TransitionTo(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_NULL_HANDLE);
	pTexture->UploadData(compressed);
	pTexture->TransitionTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);

	Console::WriteLine("Finished loading file \"{}\"", Console::Severity::Debug, file);
	return pTexture.release();
}

Texture* Texture::LoadFromInternalFormat(const std::span<const char>& data, bool useMipMaps, TextureUseCase useCase)
{
	ktx_error_code_e err = KTX_SUCCESS;

	std::unique_ptr<Texture> pReturn(new Texture());

	ktxTexture2* pRaw = nullptr;

	err = ktxTexture2_CreateFromMemory(reinterpret_cast<const ktx_uint8_t*>(data.data()), data.size(), KTX_TEXTURE_CREATE_ALLOC_STORAGE, &pRaw);
	std::unique_ptr<ktxTexture2, KtxTextureDeleter> pTexture(pRaw);

	if (err != KTX_SUCCESS)
		return nullptr;

	pReturn->CreateWithCustomSize(pRaw->baseWidth, pRaw->baseHeight, pRaw->dataSize, 1, static_cast<VkFormat>(pRaw->vkFormat), TEXTURE_USAGE, None);

	const char* asChar = reinterpret_cast<const char*>(pRaw->pData);
			
	pReturn->TransitionTo(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_NULL_HANDLE);
	pReturn->UploadData({ asChar, asChar + pRaw->dataSize });
	pReturn->TransitionTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_NULL_HANDLE);

	return pReturn.release();
}

void Texture::GeneratePlaceholderTextures()
{
	placeholderAlbedo = LoadFromForeignFormat("textures/placeholderAlbedo.png", Type::Albedo, false);
	placeholderNormal = LoadFromForeignFormat("textures/placeholderNormal.png", Type::Normal, false);
	placeholderMetallic = LoadFromForeignFormat("textures/placeholderMetallic.png", Type::Metallic, false);
	placeholderRoughness = LoadFromForeignFormat("textures/placeholderRoughness.png", Type::Roughness, false);
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

void Image::SetLayout(VkImageLayout layout)
{
	currLayout = layout;
}

void Image::TransitionTo(VkImageLayout newLayout, CommandBuffer cmdBuffer)
{
	if (newLayout == currLayout)
	{
		Console::WriteLine("caught a redundant image transition ({})", Console::Severity::Warning, string_VkImageLayout(currLayout));
		return;
	}
		
	const Vulkan::Context& ctx = Vulkan::GetContext();

	bool isSingleTime = !cmdBuffer.IsValid();

	VkCommandPool commandPool = VK_NULL_HANDLE;
	if (isSingleTime)
	{
		commandPool = Vulkan::FetchNewCommandPool(ctx.graphicsIndex);
		cmdBuffer = CommandBuffer(Vulkan::BeginSingleTimeCommands(commandPool));
	}

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = currLayout;
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

	if (currLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = 0;
		memoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (currLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = 0;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (currLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (currLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else if (currLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}
	else if (currLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else throw std::invalid_argument(std::format("Invalid layout transition ({} -> {})", string_VkImageLayout(currLayout), string_VkImageLayout(newLayout)));

	cmdBuffer.PipelineBarrier(sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

	if (isSingleTime)
	{
		Vulkan::EndSingleTimeCommands(ctx.graphicsQueue, cmdBuffer.Get(), commandPool);
		Vulkan::YieldCommandPool(ctx.graphicsIndex, commandPool);
	}

	currLayout = newLayout;
}

void Image::CopyBufferToImage(VkBuffer buffer)
{
	Vulkan::ExecuteSingleTimeCommands(
		[&](const CommandBuffer& cmdBuffer)
		{
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

			cmdBuffer.CopyBufferToImage(buffer, image.Get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}
	);
}

void Image::GenerateMipMaps()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(ctx.physicalDevice.Device(), format, &formatProperties);
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

uint32_t Image::GetWidth() const
{
	return width;
}

uint32_t Image::GetHeight() const
{
	return height;
}

int Image::GetMipLevels() const
{
	return mipLevels;
}

VkImageUsageFlags Image::GetUsage() const
{
	return usage;
}

void Image::Destroy()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	vgm::Delete(view);
	image.Destroy();

	view = VK_NULL_HANDLE;
	ResetAllAttributes();
}