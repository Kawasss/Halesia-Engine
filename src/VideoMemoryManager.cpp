#include <vector>
#include <string>
#include <map>
#include <set>

#include "renderer/VideoMemoryManager.h"
#include "renderer/Vulkan.h"
#include "renderer/GarbageManager.h"

#include "system/CriticalSection.h"

#include <iostream>

constexpr VkDeviceSize STANDARD_MEMORY_BLOCK_SIZE = 1024 * 1024; // 1 mb

enum UsageType
{
	USAGE_TYPE_IMAGE,
	USAGE_TYPE_BUFFER,
};

struct Segment
{
	bool empty = true;
	VkDeviceSize begin, end;

	VkDeviceSize GetSize() { return end - begin; }

	bool operator<(const Segment& other) { return begin < other.begin&& end < other.end; }
};

struct MemoryBlock
{
	MemoryBlock() = default;
	MemoryBlock(UsageType us, VkMemoryPropertyFlags p, VkDeviceSize s, VkDeviceSize u, VkDeviceSize a, VkDeviceMemory m) : usageType(us), properties(p), size(s), used(u), alignment(a), memory(m) {}

	UsageType usageType;
	VkMemoryPropertyFlags properties;
	VkDeviceSize size;
	VkDeviceSize used;
	VkDeviceSize alignment;
	VkDeviceMemory memory;

	int mappedCount = 0;
	void* mapped = nullptr;

	std::map<uint64_t, Segment> segments;

	VkDeviceSize GetLeftOverMemory() { return size - used; }

	void CheckedMap(VkMemoryMapFlags flags)
	{
		mappedCount++;
		if (mappedCount <= 1 || mapped == nullptr)
			ForceMap(flags);
	}

	void ForceMap(VkMemoryMapFlags flags)
	{
		const Vulkan::Context& ctx = Vulkan::GetContext();
		VkResult result = vkMapMemory(ctx.logicalDevice, memory, 0, VK_WHOLE_SIZE, flags, &mapped); // map the entire block, so the offset must be 0
		CheckVulkanResult("Failed to map a memory block", result, vkMapMemory);
	}

	void CheckedUnmap()
	{
		mappedCount--;
		if (mappedCount <= 0)
			ForceUnmap();
	}

	void ForceUnmap()
	{
		vkUnmapMemory(Vulkan::GetContext().logicalDevice, memory);
		mapped = nullptr;
	}

	std::map<uint64_t, Segment>::iterator FindUsableSegment(VkDeviceSize size)
	{
		auto it = segments.begin();

		for (; it != segments.end(); it++)
			if (it->second.empty && it->second.GetSize() >= size)
				return it;
		return it;
	}

	bool IsEmpty() { return segments.size() == 0; }
};

win32::CriticalSection blockGuard;
std::vector<MemoryBlock*> blocks;

std::map<VkBuffer, MemoryBlock*> bufferToBlock;
std::map<VkImage, MemoryBlock*>  imageToBlock;

inline VkDeviceSize FitSizeToAlignment(VkDeviceSize size, VkDeviceSize alignment)
{
	return size < alignment ? alignment : size;
}

MemoryBlock* FindRelevantMemoryBlock(VkBuffer buffer)
{
	if (bufferToBlock.find(buffer) == bufferToBlock.end())
		throw VulkanAPIError("Failed to find a relevant memory for the given handle");
	return bufferToBlock[buffer];
}

MemoryBlock* FindRelevantMemoryBlock(VkImage image)
{
	if (imageToBlock.find(image) == imageToBlock.end())
		throw VulkanAPIError("Failed to find a relevant memory for the given handle");
	return imageToBlock[image];
}

MemoryBlock* AllocateNewBlock(VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties, UsageType usageType, void* pNext = nullptr)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkDeviceSize size = requirements.size < STANDARD_MEMORY_BLOCK_SIZE ? STANDARD_MEMORY_BLOCK_SIZE : requirements.size;

	VkMemoryAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext = pNext;
	allocateInfo.allocationSize = size;
	allocateInfo.memoryTypeIndex = Vulkan::GetMemoryType(requirements.memoryTypeBits, properties, ctx.physicalDevice);

	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkResult result = vkAllocateMemory(ctx.logicalDevice, &allocateInfo, nullptr, &memory);
	CheckVulkanResult("Failed to allocate " + std::to_string(requirements.size) + " bytes of memory", result, vkAllocateMemory);

	MemoryBlock* block = new MemoryBlock(usageType, properties, size, 0, requirements.alignment, memory);

	blocks.push_back(block);

	std::cout << "Created new memory block with size " << allocateInfo.allocationSize / 1024 << " kb\n";

	return block;
}

inline MemoryBlock* GetMemoryBlock(VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties, UsageType usageType, void* pNext = nullptr)
{
	/*for (int i = 0; i < blocks.size(); i++)
	{
		MemoryBlock* block = blocks[i];
		if (block->properties == properties && block->GetLeftOverMemory() >= requirements.size && block->usageType == usageType && block->alignment == requirements.alignment)
			return block;
	}*/
	return AllocateNewBlock(requirements, properties, usageType, pNext);
}

VvmImage VideoMemoryManager::AllocateImage(VkImage image, VkMemoryPropertyFlags properties)
{
	win32::CriticalLockGuard guard(blockGuard);
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(ctx.logicalDevice, image, &requirements);
	requirements.size = FitSizeToAlignment(requirements.size, requirements.alignment);

	MemoryBlock* block = GetMemoryBlock(requirements, properties, USAGE_TYPE_IMAGE);

	uint64_t handle = reinterpret_cast<uint64_t>(image);
	auto it = block->FindUsableSegment(requirements.size);

	if (block->IsEmpty())
		block->segments[handle] = { false, 0, requirements.size };
	else if (it != block->segments.end())
	{
		auto pair = block->segments.extract(it->first); // effectively update the key and value in place

		pair.key() = handle;
		pair.mapped().end = pair.mapped().begin + requirements.size;
		pair.mapped().empty = false;
	}

	vkBindImageMemory(ctx.logicalDevice, image, block->memory, block->used);

	block->used += requirements.size;
	imageToBlock[image] = block;

	return VvmImage(image);
}

VvmBuffer VideoMemoryManager::AllocateBuffer(VkBuffer buffer, VkMemoryPropertyFlags properties, void* pNext)
{
	win32::CriticalLockGuard guard(blockGuard);
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(ctx.logicalDevice, buffer, &requirements);

	MemoryBlock* block = GetMemoryBlock(requirements, properties, USAGE_TYPE_BUFFER, pNext);

	uint64_t handle = reinterpret_cast<uint64_t>(buffer);
	auto it = block->FindUsableSegment(requirements.size);

	if (block->IsEmpty())
		block->segments[handle] = { false, 0, requirements.size };
	else if (it != block->segments.end())
	{
		auto pair = block->segments.extract(it->first); // effectively update the key and value in place

		pair.key() = handle;
		pair.mapped().end = pair.mapped().begin + requirements.size;
		pair.mapped().empty = false;
	}

	vkBindBufferMemory(ctx.logicalDevice, buffer, block->memory, block->used);

	block->used += requirements.size;
	bufferToBlock[buffer] = block;

	std::cout << std::hex << (uint64_t)buffer << '\n';
	return VvmBuffer(buffer);
}

void* VideoMemoryManager::MapBuffer(VvmBuffer buffer, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	win32::CriticalLockGuard guard(blockGuard);

	MemoryBlock* block = FindRelevantMemoryBlock(buffer.Get());
	block->CheckedMap(flags);

	uint64_t handle = reinterpret_cast<uint64_t>(buffer.Get());
	offset += block->segments[handle].begin; // can also check for incorrect offsets here

	char* data = reinterpret_cast<char*>(block->mapped);
	return reinterpret_cast<void*>(data + offset); // add the offset afterwards, since its the entire block thats mapped and not just this instance
}

void VideoMemoryManager::UnmapBuffer(VvmBuffer buffer)
{
	win32::CriticalLockGuard guard(blockGuard);

	MemoryBlock* block = FindRelevantMemoryBlock(buffer.Get());
	block->CheckedUnmap();
}

void VideoMemoryManager::Destroy(VkImage image)
{
	win32::CriticalLockGuard guard(blockGuard);
	uint64_t handle = reinterpret_cast<uint64_t>(image);

	vgm::Delete(image);
	MemoryBlock* block = FindRelevantMemoryBlock(image);
	block->segments[handle].empty = true;
}

void VideoMemoryManager::Destroy(VkBuffer buffer)
{
	win32::CriticalLockGuard guard(blockGuard);
	uint64_t handle = reinterpret_cast<uint64_t>(buffer);

	vgm::Delete(buffer);
	MemoryBlock* block = FindRelevantMemoryBlock(buffer);
	block->segments[handle].empty = true;
}

void VideoMemoryManager::ForceDestroy()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	for (MemoryBlock* block : blocks)
	{
		vgm::Delete(block->memory);
		delete block;
	}
}