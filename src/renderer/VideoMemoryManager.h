#pragma once
#include <vulkan/vulkan.h>

struct VvmBuffer;
struct VvmImage;

class VideoMemoryManager
{
public:
	static VvmImage  AllocateImage(VkImage image, VkMemoryPropertyFlags properties);
	static VvmBuffer AllocateBuffer(VkBuffer buffer, VkMemoryPropertyFlags properties, void* pNext = nullptr);

	static void* MapBuffer(VvmBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags flags = 0);
	static void UnmapBuffer(VvmBuffer buffer);

	static void ForceDestroy();

	static void Destroy(VkImage image);
	static void Destroy(VkBuffer buffer);
};

struct VvmBuffer
{
	VvmBuffer() = default;
	VvmBuffer(VkBuffer buffer) : buffer(buffer) {}

	void Destroy() 
	{ 
		if (!IsValid())
			return;

		VideoMemoryManager::Destroy(buffer); 
		Invalidate();
	}

	VkBuffer& Get() { return buffer; }
	const VkBuffer& Get() const { return buffer; }

	bool IsValid() { return buffer != VK_NULL_HANDLE; }
	void Invalidate() { buffer = VK_NULL_HANDLE; }

private:
	VkBuffer buffer;
};

struct VvmImage
{
public:
	VvmImage() = default;
	VvmImage(VkImage image) : image(image) {}

	void Destroy()
	{ 
		if (!IsValid())
			return;

		VideoMemoryManager::Destroy(image); 
		Invalidate();
	}

	VkImage& Get() { return image; }
	const VkImage& Get() const { return image; }

	bool IsValid() { return image != VK_NULL_HANDLE; }
	void Invalidate() { image = VK_NULL_HANDLE; }

private:
	VkImage image;
};