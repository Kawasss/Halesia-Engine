#include <cassert>

#include "renderer/VideoMemoryManager.h"
#include "renderer/VulkanAPIError.h"

#include "system/CriticalSection.h"

import std;

import Renderer.VulkanGarbageManager;
import Renderer.Vulkan;

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
		VkResult result = vkMapMemory(ctx.logicalDevice, memory, 0, VK_WHOLE_SIZE, flags, &mapped);
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

	void AddNewSegment(uint64_t handle, const Segment& segment)
	{
		segments.emplace(handle, segment);
	}

	void OverrideIterator(const std::map<uint64_t, Segment>::iterator& it, uint64_t handle, VkDeviceSize size)
	{
		auto nh = segments.extract(it);
		nh.key() = handle;
		nh.mapped().SetSize(size);
		nh.mapped().empty = false;

		segments.insert(std::move(nh));
	}

	bool TryToFitNewSegment(uint64_t handle, VkDeviceSize size)
	{
		VkDeviceSize farEnd = 0;
		for (auto it = segments.begin(); it != segments.end(); it++)
		{
			const Segment& segment = it->second;
			farEnd = std::max(segment.end, farEnd);

			if (!segment.empty || segment.GetSize() < size) // can try a more clever algorithm that finds the memory which size is closest to the desired memory
				continue;
		
			OverrideIterator(it, handle, size);
			return true;
		}

		if (farEnd + size > this->size)
			return false;

		AddNewSegment(handle, Segment(false, farEnd, farEnd + size));
		return true;
	}

	void FreeSegment(uint64_t handle)
	{
		segments[handle].empty = true;
	}

	bool IsEmpty() const
	{
		for (const auto& [id, segment] : segments)
			if (!segment.empty)
				return false;
		return true;
	}

	bool IsUnused() const { return segments.size() == 0; }

	void Name(const std::string_view& name) const
	{
		Vulkan::SetDebugName(memory, name.data());
	}

	void Destroy() const
	{
		vgm::Delete(memory);
	}

	~MemoryBlock()
	{
		Destroy();
	}

	private:
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

	requirements.size = ::GetAppropriateBlockSize(requirements.size);

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
	VkDeviceMemory memory = ::AllocateMemory(requirements, properties, pNext);
	vvm::MemoryBlock* block = new vvm::MemoryBlock(properties, requirements.size, requirements.alignment, memory);
	
	core->blocks.push_back(block);
	return block;
}

static vvm::MemoryBlock* FindValidBlockForSegment(VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties, uint64_t handle, VkDeviceSize size, void* pNext = nullptr) // will allocate a new block if it does not find a valid one
{
	for (vvm::MemoryBlock* pBlock : core->blocks)
	{
		if (pBlock->properties != properties || pBlock->alignment != requirements.alignment)
			continue;

		if (pBlock->TryToFitNewSegment(handle, size))
			return pBlock;
	}

	vvm::MemoryBlock* pBlock = ::AllocateNewBlock(requirements, properties, pNext);
	if (!pBlock->TryToFitNewSegment(handle, size))
		__debugbreak();
	return pBlock;
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
	requirements.size = ::FitSizeToAlignment(requirements.size, requirements.alignment);

	VkDeviceSize imageSize = requirements.size;
	uint64_t handle = reinterpret_cast<uint64_t>(image);

	MemoryBlock* block = ::FindValidBlockForSegment(requirements, properties, handle, imageSize);//::GetMemoryBlock(requirements, properties);

	vkBindImageMemory(ctx.logicalDevice, image, block->memory, block->segments[handle].begin);

	block->Name(std::format("image_mem_block:{}", block->size));

	core->imageToBlock.emplace(image, block);
	return Image(image);
}

vvm::Buffer vvm::AllocateBuffer(VkBuffer buffer, VkMemoryPropertyFlags properties, void* pNext)
{
	win32::CriticalLockGuard guard(core->blockGuard);
	const Vulkan::Context& ctx = Vulkan::GetContext();

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(ctx.logicalDevice, buffer, &requirements);

	VkDeviceSize bufferSize = requirements.size;
	uint64_t handle = reinterpret_cast<uint64_t>(buffer);

	MemoryBlock* block = ::FindValidBlockForSegment(requirements, properties, handle, bufferSize, pNext);//::GetMemoryBlock(requirements, properties, pNext);

	vkBindBufferMemory(ctx.logicalDevice, buffer, block->memory, block->segments[handle].begin);

	block->Name(std::format("buffer_mem_block:{}", block->size));

	core->bufferToBlock.emplace(buffer, block);
	return Buffer(buffer);
}

void* vvm::MapBuffer(Buffer buffer, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	win32::CriticalLockGuard guard(core->blockGuard);

	MemoryBlock* block = ::FindRelevantMemoryBlock(buffer.Get());
	block->CheckedMap(size, offset, flags);

	uint64_t handle = reinterpret_cast<uint64_t>(buffer.Get());
	offset += block->segments[handle].begin; // can also check for incorrect offsets here

	char* data = static_cast<char*>(block->mapped);
	return static_cast<void*>(data + offset); // add the offset afterwards, since its the entire block thats mapped and not just this instance
}

void vvm::UnmapBuffer(Buffer buffer)
{
	win32::CriticalLockGuard guard(core->blockGuard);

	MemoryBlock* block = ::FindRelevantMemoryBlock(buffer.Get());
	block->CheckedUnmap();
}

void vvm::Destroy(VkImage image)
{
	win32::CriticalLockGuard guard(core->blockGuard);
	uint64_t handle = reinterpret_cast<uint64_t>(image);

	vgm::Delete(image);
	MemoryBlock* block = ::FindRelevantMemoryBlock(image);
	block->FreeSegment(handle);
	::CheckBlockStatus(block, core->imageToBlock);
}

void vvm::Destroy(VkBuffer buffer)
{
	win32::CriticalLockGuard guard(core->blockGuard);
	uint64_t handle = reinterpret_cast<uint64_t>(buffer);

	vgm::Delete(buffer);
	MemoryBlock* block = ::FindRelevantMemoryBlock(buffer);
	block->FreeSegment(handle);
	::CheckBlockStatus(block, core->bufferToBlock);
}

void vvm::ForceDestroy()
{
	win32::CriticalLockGuard guard(core->blockGuard);
	for (MemoryBlock* block : core->blocks)
	{
		delete block;
	}
}

size_t vvm::GetBlockCount()
{
	win32::CriticalLockGuard guard(core->blockGuard);
	return core->blocks.size();
}

size_t vvm::GetAllocatedByteCount()
{
	size_t ret = 0;

	win32::CriticalLockGuard guard(core->blockGuard);
	for (const vvm::MemoryBlock* block : core->blocks)
	{
		ret += block->size;
	}

	return ret;
}

static vvm::DbgMemoryBlock ConvertMemoryBlock(const vvm::MemoryBlock* memoryBlock)
{
	vvm::DbgMemoryBlock block{};
	block.segments.reserve(memoryBlock->segments.size());
	block.flags = memoryBlock->properties;
	block.alignment = memoryBlock->alignment;
	block.size = memoryBlock->size;

	for (const auto& [handle, segment] : memoryBlock->segments)
	{
		vvm::DbgSegment dbgSegment{};
		dbgSegment.begin = segment.begin;
		dbgSegment.end = segment.end;

		block.used += dbgSegment.end - dbgSegment.begin;
		block.segments.push_back(std::move(dbgSegment));
	}
	return block;
}

std::vector<vvm::DbgMemoryBlock> vvm::DbgGetMemoryBlocks()
{
	win32::CriticalLockGuard guard(core->blockGuard);

	std::vector<DbgMemoryBlock> ret;
	ret.reserve(core->bufferToBlock.size() + core->imageToBlock.size());

	for (const auto& [buffer, memoryBlock] : core->bufferToBlock)
	{
		ret.push_back(ConvertMemoryBlock(memoryBlock));
	}

	for (const auto& [image, memoryBlock] : core->imageToBlock)
	{
		ret.push_back(ConvertMemoryBlock(memoryBlock));
	}
	return ret;
}