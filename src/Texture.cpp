#define NOMINMAX
#include <fstream>
#include "renderer/Vulkan.h"
#include "renderer/physicalDevice.h"
#include "renderer/Texture.h"
#include "Console.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

bool Image::texturesHaveChanged = false;
Texture* Texture::placeholderAlbedo = nullptr;
Texture* Texture::placeholderNormal = nullptr;
Texture* Texture::placeholderMetallic = nullptr;
Texture* Texture::placeholderRoughness = nullptr;
Texture* Texture::placeholderAmbientOcclusion = nullptr;

bool Image::TexturesHaveChanged()
{
	bool ret = texturesHaveChanged;
	texturesHaveChanged = false;
	return ret;
}

void Image::GenerateImages(const TextureCreationObject& creationObjects, std::vector<std::vector<char>>& textureData, bool useMipMaps)
{
	this->logicalDevice = creationObjects.logicalDevice;
	this->commandPool = Vulkan::FetchNewCommandPool(creationObjects);
	this->queue = creationObjects.queue;
	this->physicalDevice = creationObjects.physicalDevice;

	layerCount = static_cast<uint32_t>(textureData.size());
	int textureChannels = 0;

	if (useMipMaps)
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
	stbi_set_flip_vertically_on_load(1);
	// read every side of the cubemap
	std::vector<stbi_uc*> pixels(layerCount);
	for (uint32_t i = 0; i < layerCount; i++)
		pixels[i] = stbi_load_from_memory((unsigned char*)textureData[i].data(), (int)textureData[i].size(), &width, &height, &textureChannels, STBI_rgb_alpha);

	VkDeviceSize layerSize = width * height * 4;
	VkDeviceSize imageSize = layerSize * layerCount;

	if (layerSize == 0)
		throw std::runtime_error("Invalid image found: layer size is 0");

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	
	Vulkan::CreateBuffer(logicalDevice, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// copy all of the different sides of the cubemap into a single buffer
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
	for (uint32_t i = 0; i < layerCount; i++)
		memcpy((char*)data + (layerSize * i), pixels[i], static_cast<size_t>(imageSize)); // cast the void* to char for working arithmetic
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	for (uint32_t i = 0; i < layerCount; i++) // dont know if this can be put in the other for loop used for reading the files
		stbi_image_free(pixels[i]);

	VkImageCreateFlags flags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	Vulkan::CreateImage(logicalDevice, physicalDevice, width, height, mipLevels, layerCount, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, flags, image, imageMemory);
	
	TransitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	VkImageViewType viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	imageView = Vulkan::CreateImageView(logicalDevice, image, viewType, mipLevels, layerCount, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	if (useMipMaps) GenerateMipMaps(VK_FORMAT_R8G8B8A8_SRGB);

	Vulkan::YieldCommandPool(creationObjects.queueIndex, commandPool);

	this->texturesHaveChanged = true;
}

void Image::GenerateEmptyImages(const TextureCreationObject& creationObjects, int width, int height, int amount)
{
	this->logicalDevice = creationObjects.logicalDevice;
	this->commandPool = creationObjects.commandPool;
	this->queue = creationObjects.queue;
	this->physicalDevice = creationObjects.physicalDevice;
	this->width = width;
	this->height = height;
	this->layerCount = static_cast<uint32_t>(amount);

	VkDeviceSize layerSize = width * height * 4;
	VkDeviceSize imageSize = layerSize * amount;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	Vulkan::CreateBuffer(logicalDevice, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	VkImageCreateFlags flags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	Vulkan::CreateImage(logicalDevice, physicalDevice, width, height, mipLevels, layerCount, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, flags, image, imageMemory);

	TransitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	VkImageViewType viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	imageView = Vulkan::CreateImageView(logicalDevice, image, viewType, mipLevels, layerCount, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

	this->texturesHaveChanged = true;
}

void Image::AwaitGeneration()
{
	generation.get();
}

bool Image::HasFinishedLoading()
{
	return generation._Is_ready();
}

std::vector<char> ReadFile(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		throw std::runtime_error("Failed to open the file at " + filePath);

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();
	return buffer;
}

std::vector<std::vector<char>> GetAllImageData(std::vector<std::string> filePaths) // dont know if the copy speed is a concern
{
	std::vector<std::vector<char>> fileDatas;
	for (int i = 0; i < filePaths.size(); i++)
		fileDatas.push_back(ReadFile(filePaths[i]));
	return fileDatas;
}

Cubemap::Cubemap(const TextureCreationObject& creationObjects, std::vector<std::string> filePath, bool useMipMaps)
{
	if (filePath.size() != 6) 
		throw new std::runtime_error("Invalid amount of images given for a cubemap: expected 6, but got " + std::to_string(filePath.size()));

	// this async can't use the capture list ( [&] ), because then filePath gets wiped clean (??)
	generation = std::async([](Cubemap* cubemap, const TextureCreationObject& creationObjects, std::vector<std::string> filePath, bool useMipMaps)
		{
			std::vector<std::vector<char>> data = GetAllImageData(filePath);
			cubemap->GenerateImages(creationObjects, data, useMipMaps);
		}, this, creationObjects, filePath, useMipMaps);
}

Texture::Texture(const TextureCreationObject& creationObjects, std::string filePath, bool useMipMaps)
{
	// this async can't use the capture list ( [&] ), because then filePath gets wiped clean (??)
	generation = std::async([](Texture* texture, const TextureCreationObject& creationObjects, std::string filePath, bool useMipMaps)
		{
			std::vector<std::vector<char>> data = GetAllImageData({ filePath });
			texture->GenerateImages(creationObjects, data, useMipMaps);
		}, this, creationObjects, filePath, useMipMaps);
}

Texture::Texture(const TextureCreationObject& creationObjects, std::vector<char> imageData, bool useMipMaps)
{
	if (imageData.size() <= 0)
		throw std::runtime_error("Invalid texture size: imageData.size <= 0");

	generation = std::async([](Texture* texture, const TextureCreationObject& creationObjects, std::vector<char> imageData, bool useMipMaps) 
		{
			std::vector<std::vector<char>> data{ imageData };
			texture->GenerateImages(creationObjects, data, useMipMaps);
		}, this, creationObjects, imageData, useMipMaps);
}

void Texture::GeneratePlaceholderTextures(const TextureCreationObject& creationObjects)
{
	placeholderAlbedo = new Texture(creationObjects, "textures/placeholderAlbedo.png", false);
	placeholderNormal = new Texture(creationObjects, "textures/placeholderNormal.png", false);
	placeholderMetallic = new Texture(creationObjects, "textures/placeholderMetallic.png", false);
	placeholderRoughness = new Texture(creationObjects, "textures/placeholderRoughness.png", false);
	placeholderAmbientOcclusion = new Texture(creationObjects, "textures/placeholderAO.png", false);
}

void Texture::DestroyPlaceholderTextures()
{
	placeholderAlbedo->Destroy();
	placeholderNormal->Destroy();
	placeholderMetallic->Destroy();
	placeholderRoughness->Destroy();
	placeholderAmbientOcclusion->Destroy();
}

void Image::TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.oldLayout = oldLayout;
	memoryBarrier.newLayout = newLayout;
	memoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memoryBarrier.image = image;
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
	else throw std::invalid_argument("Invalid layout transition: " + (std::string)string_VkImageLayout(newLayout));

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);

	Vulkan::EndSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);
}

void Image::CopyBufferToImage(VkBuffer buffer)
{
	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);

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

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	Vulkan::EndSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);
}

void Image::GenerateMipMaps(VkFormat imageFormat)
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice.Device(), imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		throw std::runtime_error("Failed to find support for optimal tiling with the given format");

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);

	VkImageMemoryBarrier memoryBarrier{};
	memoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memoryBarrier.image = image;
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

		vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

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

	Vulkan::EndSingleTimeCommands(logicalDevice, queue, commandBuffer, commandPool);
}

int Image::GetWidth()
{
	return width;
}

int Image::GetHeight()
{
	return height;
}

int Image::GetMipLevels()
{
	return mipLevels;
}

void Image::Destroy()
{
	this->texturesHaveChanged = true;
	vkDestroyImageView(logicalDevice, imageView, nullptr);
	vkDestroyImage(logicalDevice, image, nullptr);
	vkFreeMemory(logicalDevice, imageMemory, nullptr);
	delete this;
}