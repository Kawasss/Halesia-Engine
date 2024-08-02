#include <fstream>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "renderer/Vulkan.h"
#include "renderer/physicalDevice.h"
#include "renderer/Texture.h"
#include "core/Console.h"

#include "tools/common.h"

bool Image::texturesHaveChanged = false;
Texture* Texture::placeholderAlbedo = nullptr;
Texture* Texture::placeholderNormal = nullptr;
Texture* Texture::placeholderMetallic = nullptr;
Texture* Texture::placeholderRoughness = nullptr;
Texture* Texture::placeholderAmbientOcclusion = nullptr;

uint8_t* Color::GetData() const
{
	return (uint8_t*)this;
}

bool Image::TexturesHaveChanged()
{
	bool ret = texturesHaveChanged;
	texturesHaveChanged = false;
	return ret;
}

void Image::GenerateImages(std::vector<std::vector<char>>& textureData, bool useMipMaps, TextureFormat format, TextureUseCase useCase)
{
	const Vulkan::Context context = Vulkan::GetContext();
	this->logicalDevice = context.logicalDevice;
	this->commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);
	this->queue = context.graphicsQueue;
	this->physicalDevice = context.physicalDevice;
	
	layerCount = static_cast<uint32_t>(textureData.size());
	int textureChannels = 0;

	if (useMipMaps)
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
	stbi_set_flip_vertically_on_load(1);
	// read every side of the cubemap
	std::vector<stbi_uc*> pixels(layerCount);
	for (uint32_t i = 0; i < layerCount; i++)
		pixels[i] = stbi_load_from_memory((unsigned char*)textureData[i].data(), (int)textureData[i].size(), &width, &height, &textureChannels, STBI_rgb_alpha);

	WritePixelsToBuffer(pixels, useMipMaps, format, (VkImageLayout)useCase);

	for (uint32_t i = 0; i < layerCount; i++)
		stbi_image_free(pixels[i]);
}

void Image::GenerateEmptyImages(int width, int height, int amount)
{
	const Vulkan::Context context = Vulkan::GetContext();
	this->logicalDevice = context.logicalDevice;
	this->commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);
	this->queue = context.graphicsQueue;
	this->physicalDevice = context.physicalDevice;
	this->width = width;
	this->height = height;
	this->layerCount = static_cast<uint32_t>(amount);

	VkDeviceSize layerSize = width * height * 4;
	VkDeviceSize imageSize = layerSize * amount;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	Vulkan::CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	VkImageCreateFlags flags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	Vulkan::CreateImage(width, height, mipLevels, layerCount, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, flags, image, imageMemory);

	TransitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	VkImageViewType viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	imageView = Vulkan::CreateImageView(image, viewType, mipLevels, layerCount, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

	Vulkan::YieldCommandPool(context.graphicsIndex, commandPool);

	this->texturesHaveChanged = true;
}

void Image::WritePixelsToBuffer(const std::vector<uint8_t*>& pixels, bool useMipMaps, TextureFormat format, VkImageLayout layout)
{
	const Vulkan::Context context = Vulkan::GetContext();

	VkDeviceSize layerSize = width * height * 4;
	size = layerSize * layerCount;

	if (layerSize == 0)
		throw std::runtime_error("Invalid image found: layer size is 0");

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	std::lock_guard<std::mutex> lockGuard(Vulkan::graphicsQueueMutex);
	Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	// copy all of the different sides of the cubemap into a single buffer
	void* data;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, size, 0, &data);
	for (uint32_t i = 0; i < layerCount; i++)
		memcpy((char*)data + (layerSize * i), pixels[i], static_cast<size_t>(size)); // cast the void* to char for working arithmetic
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	VkImageCreateFlags flags = layerCount == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
	Vulkan::CreateImage(width, height, mipLevels, layerCount, (VkFormat)format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, flags, image, imageMemory);

	TransitionImageLayout((VkFormat)format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

	VkImageViewType viewType = layerCount == 6 ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
	imageView = Vulkan::CreateImageView(image, viewType, mipLevels, layerCount, (VkFormat)format, VK_IMAGE_ASPECT_COLOR_BIT);
	if (useMipMaps) 
		GenerateMipMaps((VkFormat)format);
	else
		TransitionImageLayout((VkFormat)format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);

	Vulkan::YieldCommandPool(context.graphicsIndex, commandPool);

	this->texturesHaveChanged = true;
}

void Image::ChangeData(uint8_t* data, uint32_t size, TextureFormat format)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	std::lock_guard<std::mutex> lockGuard(Vulkan::graphicsQueueMutex);
	Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* ptr;
	vkMapMemory(logicalDevice, stagingBufferMemory, 0, size, 0, &ptr);
	memcpy(ptr, data, size);
	vkUnmapMemory(logicalDevice, stagingBufferMemory);

	TransitionImageLayout((VkFormat)format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	CopyBufferToImage(stagingBuffer);

	vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

std::vector<uint8_t> Image::GetImageData()
{
	AwaitGeneration();

	const Vulkan::Context& context = Vulkan::GetContext();
	VkCommandPool commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);

	VkBuffer copyBuffer;
	VkDeviceMemory copyMemory;
	Vulkan::CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, copyBuffer, copyMemory);

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
	memoryBarrier.image = image;
	memoryBarrier.subresourceRange = subresourceRange;
	
	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool); // messy
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
	Vulkan::EndSingleTimeCommands(context.graphicsQueue, commandBuffer, commandPool);

	commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);
	vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, copyBuffer, 1, &imageCopy);
	Vulkan::EndSingleTimeCommands(context.graphicsQueue, commandBuffer, commandPool);

	memoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	memoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool); // still messy
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &memoryBarrier);
	Vulkan::EndSingleTimeCommands(context.graphicsQueue, commandBuffer, commandPool);

	Vulkan::YieldCommandPool(context.graphicsIndex, commandPool);

	uint8_t* ptr = nullptr;
	vkMapMemory(logicalDevice, copyMemory, 0, size, 0, (void**)&ptr);

	std::vector<uint8_t> ret;
	ret.resize(size / sizeof(uint8_t));
	memcpy(ret.data(), ptr, size);

	vkUnmapMemory(logicalDevice, copyMemory);
	vkDestroyBuffer(logicalDevice, copyBuffer, nullptr);
	vkFreeMemory(logicalDevice, copyMemory, nullptr);

	return ret;
}

void Image::AwaitGeneration()
{
	if (generation.valid())
		generation.get();
}

bool Image::HasFinishedLoading()
{
	if (generation.valid() && generation.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		generation.get(); // change the status of the image if it it done loading
	return !generation.valid();
}

std::vector<std::vector<char>> GetAllImageData(std::vector<std::string> filePaths) // dont know if the copy speed is a concern
{
	std::vector<std::vector<char>> fileDatas;
	for (int i = 0; i < filePaths.size(); i++)
		fileDatas.push_back(ReadFile(filePaths[i]));
	return fileDatas;
}

Cubemap::Cubemap(std::vector<std::string> filePath, bool useMipMaps)
{
	if (filePath.size() != 6) 
		throw new std::runtime_error("Invalid amount of images given for a cubemap: expected 6, but got " + std::to_string(filePath.size()));

	// this async can't use the capture list ( [&] ), because then filePath gets wiped clean (??)
	generation = std::async([](Cubemap* cubemap, std::vector<std::string> filePath, bool useMipMaps)
		{
			std::vector<std::vector<char>> data = GetAllImageData(filePath);
			cubemap->GenerateImages(data, useMipMaps);
		}, this, filePath, useMipMaps);
}

Texture::Texture(std::string filePath, bool useMipMaps, TextureFormat format, TextureUseCase useCase)
{
	// this async can't use the capture list ( [&] ), because then filePath gets wiped clean (??)
	generation = std::async([](Texture* texture, std::string filePath, bool useMipMaps, TextureFormat format, TextureUseCase useCase)
		{
			std::vector<std::vector<char>> data = GetAllImageData({ filePath });
			texture->GenerateImages(data, useMipMaps, format, useCase);
		}, this, filePath, useMipMaps, format, useCase);
}

Texture::Texture(std::vector<char> imageData, uint32_t width, uint32_t height, bool useMipMaps, TextureFormat format, TextureUseCase useCase)
{
	if (imageData.empty())
		throw std::runtime_error("Invalid texture size: imageData.empty()");

	const Vulkan::Context context = Vulkan::GetContext();
	this->logicalDevice = context.logicalDevice;
	this->commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);
	this->queue = context.graphicsQueue;
	this->physicalDevice = context.physicalDevice;
	this->width = width;
	this->height = height;
	this->layerCount = 1;

	//generation = std::async([](Texture* texture, std::vector<char> imageData, bool useMipMaps, TextureFormat format) 
		//{
			std::vector<uint8_t*> data{ (uint8_t*)imageData.data() };
			WritePixelsToBuffer(data, useMipMaps, format, (VkImageLayout)useCase);
			texturesHaveChanged = true;
		//}, this, imageData, useMipMaps, format);
}

Texture::Texture(const Color& color, TextureFormat format, TextureUseCase useCase)
{
	const Vulkan::Context context = Vulkan::GetContext();
	logicalDevice = context.logicalDevice;
	commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);
	queue = context.graphicsQueue;
	physicalDevice = context.physicalDevice;
	width = 1, height = 1, layerCount = 1;

	generation = std::async([](Texture* texture, const Color& color, TextureFormat format, TextureUseCase useCase)
		{
			texture->WritePixelsToBuffer({ color.GetData() }, false, format, (VkImageLayout)useCase);
		}, this, color, format, useCase);
}

void Texture::GeneratePlaceholderTextures()
{
	placeholderAlbedo = new Texture("textures/placeholderAlbedo.png", false);
	placeholderNormal = new Texture("textures/placeholderNormal.png", false);
	placeholderMetallic = new Texture("textures/placeholderMetallic.png", false);
	placeholderRoughness = new Texture("textures/placeholderRoughness.png", false);
	placeholderAmbientOcclusion = new Texture("textures/placeholderAO.png", false);

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

void Image::TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);

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

	Vulkan::EndSingleTimeCommands(queue, commandBuffer, commandPool);
}

void Image::CopyBufferToImage(VkBuffer buffer)
{
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

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	Vulkan::EndSingleTimeCommands(queue, commandBuffer, commandPool);
}

void Image::GenerateMipMaps(VkFormat imageFormat)
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(physicalDevice.Device(), imageFormat, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
		throw std::runtime_error("Failed to find support for optimal tiling with the given format");

	VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(commandPool);

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

	Vulkan::EndSingleTimeCommands(queue, commandBuffer, commandPool);
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
	/*Vulkan::SubmitObjectForDeletion
	(
		[device = logicalDevice, view = imageView, image = image, memory = imageMemory]()
		{
			vkDestroyImageView(device, view, nullptr);
			vkDestroyImage(device, image, nullptr);
			vkFreeMemory(device, memory, nullptr);
		}
	);
	delete this;*/

	vkDestroyImageView(logicalDevice, imageView, nullptr);
	vkDestroyImage(logicalDevice, image, nullptr);
	vkFreeMemory(logicalDevice, imageMemory, nullptr);
}