#include <vector>
#include <string>
#include <map>
#include <set>
#include <cassert>

#include "renderer/VideoMemoryManager.h"
#include "renderer/GarbageManager.h"
#include "renderer/VulkanAPIError.h"
#include "renderer/Vulkan.h"

#include "system/CriticalSection.h"

// The video memory (vram) manager allocates large chunks of device memory and hands out parts of these chunks.
// the memory block size is dictated by the value of 'STANDARD_MEMORY_BLOCK_SIZE'. The size of a block may be bigger than the standard size to accomodate for larger buffers, 
// but the block size will never go lower than the standard size.

constexpr VkDeviceSize STANDARD_MEMORY_BLOCK_SIZE = 1024 * 1024; // 1 mb

// One block can be in use for multiple buffers or image. If some memory is freed, it will be marked as empty and can be reused for newer buffers requesting memory.
// Memory blocks allocated for buffers can not store images and vice versa.
// 
//  block 1 (buffer)       block 2 (image)
// +----------------+    +----------------+
// |  |        |####|    |        |       |
// |  |        |####| -> |        |       | -> ...
// |  |        |####|    |        |       |
// +----------------+    +----------------+
//   |       \               /         \
//   |        \             /           \
// buffer 1  buffer 2    image 1       image 2


struct vvm::Segment
{
	bool empty = true;
	VkDeviceSize begin = 0, end = 0;

	void SetSize(VkDeviceSize size) { end = begin + size; }
	VkDeviceSize GetSize() const    { return end - begin; }
};

struct vvm::MemoryBlock
{
	MemoryBlock() = default;
	MemoryBlock(VkMemoryPropertyFlags p, VkDeviceSize s, VkDeviceSize a, VkDeviceMemory m) : properties(p), size(s), alignment(a), memory(m) {}

	VkMemoryPropertyFlags properties = 0;
	VkDeviceSize size = 0;
	VkDeviceSize alignment = 0;
	VkDeviceMemory memory = VK_NULL_HANDLE;

	int mappedCount = 0;
	void* mapped = nullptr;

	std::map<uint64_t, Segment> segments;

	void CheckedMap(VkDeviceSize size, VkDeviceSize offset, VkMemoryMapFlags flags)
	{
		mappedCount++;
		if (mappedCount <= 1 || mapped == nullptr)
			ForceMap(size, offset, flags);
	}

	void ForceMap(VkDeviceSize size, VkDeviceSize offset, VkMemoryMapFlags flags)
	{
		if ((properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0)
			throw VulkanAPIError("Failed to map a memory block: it cannot be mapped due to its properties");

		const Vulkan::Context& ctx = Vulkan::GetContext();

		VkResult result = VK_SUCCESS;

		// map the entire block, because multiple segments can be mapped at the same time: vulkan only allows mapping a memory once, so the offset must be 0 and the size must be VK_WHOLE_SIZE.
		// an optimisation: if a memory block is consumed entirely by one segment, then only map the required parts. A segment that uses the entire block removes to need to check for multiple mappings.
		if (segments.size() == 1 && segments.begin()->second.GetSize() == size)
			result = vkMapMemory(ctx.logicalDevice, memory, offset, size, flags, &mapped);
		else                                  
			result = vkMapMemory(ctx.logicalDevice, memory, 0, VK_WHOLE_SIZE, flags, &mapped);

		CheckVulkanResult("Failed to map a memory block", result);
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
		for (auto it = segments.begin(); it != segments.end(); it++)
			if (it->second.empty && it->second.GetSize() >= size)
				return it;

		return segments.end();
	}

	bool CanFit(VkDeviceSize size)
	{
		return FindUsableSegment(size) != segments.end();
	}

	void FreeSegment(uint64_t handle)
	{
		segments[handle].empty = true;
	}

	bool IsEmpty() const
	{
		for (const auto& [id, segment] : segments)
			if (!segment.empty) // check if there are any used segments in this block, if no segment is used then we return true
				return false;
		return true;
	}

	bool IsUnused() const { return segments.size() == 0; }

	void Destroy() const
	{
		vgm::Delete(memory);
	}

	~MemoryBlock()
	{
		Destroy();
	}
};

struct vvm::MemoryCore
{
	win32::CriticalSection blockGuard;
	std::vector<vvm::MemoryBlock*> blocks;

	std::map<VkBuffer, vvm::MemoryBlock*> bufferToBlock; // the buffer and image blocks are seperated into 2 maps, so that for example image look-up will stay fast, even if there are a lot of buffers.
	std::map<VkImage,  vvm::MemoryBlock*> imageToBlock;
};

vvm::MemoryCore* core = nullptr;

static VkDeviceSize FitSizeToAlignment(VkDeviceSize size, VkDeviceSize alignment)
{
	return size < alignment ? alignment : size;
}

static VkDeviceSize GetAppropriateBlockSize(VkDeviceSize size)
{
	return size < STANDARD_MEMORY_BLOCK_SIZE ? STANDARD_MEMORY_BLOCK_SIZE : size;
}

static VkDeviceMemory AllocateMemory(VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties, void* pNext)
{
	const Vulkan::Context& ctx = Vulkan::GetContext();

	requirements.size = GetAppropriateBlockSize(requirements.size);

	VkMemoryAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext = pNext;
	allocateInfo.allocationSize = requirements.size;
	allocateInfo.memoryTypeIndex = Vulkan::GetMemoryType(requirements.memoryTypeBits, properties, ctx.physicalDevice);

	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkResult result = vkAllocateMemory(ctx.logicalDevice, &allocateInfo, nullptr, &memory);
	CheckVulkanResult("Failed to allocate " + std::to_string(requirements.size) + " bytes of memory", result);

	return memory;
}

static vvm::MemoryBlock* FindRelevantMemoryBlock(VkBuffer buffer)
{
	if (core->bufferToBlock.find(buffer) == core->bufferToBlock.end())
		throw VulkanAPIError("Failed to find a relevant memory for the given handle");
	return core->bufferToBlock[buffer];
}

static vvm::MemoryBlock* FindRelevantMemoryBlock(VkImage image)
{
	if (core->imageToBlock.find(image) == core->imageToBlock.end())
		throw VulkanAPIError("Failed to find a relevant memory for the given handle");
	return core->imageToBlock[image];
}

static vvm::MemoryBlock* AllocateNewBlock(VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties, void* pNext = nullptr)
{
	VkDeviceMemory memory = AllocateMemory(requirements, properties, pNext);
	vvm::MemoryBlock* block = new vvm::MemoryBlock(properties, requirements.size, requirements.alignment, memory);

	core->blocks.push_back(block);
	return block;
}

static vvm::MemoryBlock* GetMemoryBlock(VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties, void* pNext = nullptr)
{
	for (int i = 0; i < core->blocks.size(); i++)
	{
		vvm::MemoryBlock* block = core->blocks[i];
		if (block->properties == properties && block->CanFit(requirements.size) && block->alignment == requirements.alignment)
			return block;
	}
	return AllocateNewBlock(requirements, properties, pNext);
}

template<typename T>
static void CheckBlockStatus(vvm::MemoryBlock* block, std::map<T, vvm::MemoryBlock*>& map)
{
	if (!block->IsEmpty())
		return;

	for (auto it = map.begin(); it != map.end(); it++)
	{
		if (it->second != block)
			continue;

		map.erase(it);
		break;
	}
	core->blocks.erase(std::find(core->blocks.begin(), core->blocks.end(), block));
	delete block;
}

void vvm::Init()
{
	assert(core == nullptr);
	core = new MemoryCore();
}

void vvm::ShutDown()
{
	assert(core != nullptr);

	ForceDestroy();
	delete core;
}

vvm::Image vvm::AllocateImage(VkImage image, VkMemoryPropertyFlags properties)
{
	win32::CriticalLockGuard guard(core->blockGuard);
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkMemoryRequirements requirements;
	vkGetImageMemoryRequirements(ctx.logicalDevice, image, &requirements);
	requirements.size = FitSizeToAlignment(requirements.size, requirements.alignment);

	MemoryBlock* block = GetMemoryBlock(requirements, properties);

	uint64_t handle = reinterpret_cast<uint64_t>(image);
	auto it = block->FindUsableSegment(requirements.size);

	if (block->IsUnused())
		block->segments[handle] = { false, 0, requirements.size };
	else if (it != block->segments.end())
	{
		auto pair = block->segments.extract(it->first); // effectively update the key and value in place

		pair.key() = handle;
		pair.mapped().SetSize(requirements.size);
		pair.mapped().empty = false;
	}

	vkBindImageMemory(ctx.logicalDevice, image, block->memory, block->segments[handle].begin);

	core->imageToBlock[image] = block;
	return Image(image);
}

vvm::Buffer vvm::AllocateBuffer(VkBuffer buffer, VkMemoryPropertyFlags properties, void* pNext)
{
	win32::CriticalLockGuard guard(core->blockGuard);
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(ctx.logicalDevice, buffer, &requirements);

	MemoryBlock* block = GetMemoryBlock(requirements, properties, pNext);

	uint64_t handle = reinterpret_cast<uint64_t>(buffer);
	auto it = block->FindUsableSegment(requirements.size);

	if (block->IsUnused())
		block->segments[handle] = { false, 0, requirements.size };
	else if (it != block->segments.end())
	{
		auto pair = block->segments.extract(it->first); // effectively update the key and value in place

		pair.key() = handle;
		pair.mapped().SetSize(requirements.size);
		pair.mapped().empty = false;
	}

	vkBindBufferMemory(ctx.logicalDevice, buffer, block->memory, block->segments[handle].begin);

	core->bufferToBlock[buffer] = block;
	return Buffer(buffer);
}

void* vvm::MapBuffer(Buffer buffer, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	win32::CriticalLockGuard guard(core->blockGuard);

	MemoryBlock* block = FindRelevantMemoryBlock(buffer.Get());
	block->CheckedMap(size, offset, flags);

	uint64_t handle = reinterpret_cast<uint64_t>(buffer.Get());
	offset += block->segments[handle].begin; // can also check for incorrect offsets here

	char* data = reinterpret_cast<char*>(block->mapped);
	return reinterpret_cast<void*>(data + offset); // add the offset afterwards, since its the entire block thats mapped and not just this instance
}

void vvm::UnmapBuffer(Buffer buffer)
{
	win32::CriticalLockGuard guard(core->blockGuard);

	MemoryBlock* block = FindRelevantMemoryBlock(buffer.Get());
	block->CheckedUnmap();
}

void vvm::Destroy(VkImage image)
{
	win32::CriticalLockGuard guard(core->blockGuard);
	uint64_t handle = reinterpret_cast<uint64_t>(image);

	vgm::Delete(image);
	MemoryBlock* block = FindRelevantMemoryBlock(image);
	block->FreeSegment(handle);
	CheckBlockStatus(block, core->imageToBlock);
}

void vvm::Destroy(VkBuffer buffer)
{
	win32::CriticalLockGuard guard(core->blockGuard);
	uint64_t handle = reinterpret_cast<uint64_t>(buffer);

	vgm::Delete(buffer);
	MemoryBlock* block = FindRelevantMemoryBlock(buffer);
	block->FreeSegment(handle);
	CheckBlockStatus(block, core->bufferToBlock);
}

void vvm::ForceDestroy()
{
	const Vulkan::Context& ctx = Vulkan::GetContext();
	for (MemoryBlock* block : core->blocks)
	{
		delete block;
	}
}