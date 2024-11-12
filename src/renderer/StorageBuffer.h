#pragma once
#include <set>
#include <intrin.h>

#include "../system/CriticalSection.h"

#include "Vulkan.h"
#include "Buffer.h"

#include "../core/Console.h"

#define CheckHandleValidity(memory, ret)                                                                                                                   \
if (!CheckIfHandleIsValid(memory))                                                                                                                         \
{                                                                                                                                                          \
	Console::WriteLine("An invalid memory handle (" + std::to_string((uint64_t)memory) + ") has been found in " + __FUNCTION__, Console::Severity::Error); \
    __debugbreak();                                                                                                                                        \
	return ret;                                                                                                                                            \
}                                                                                                                                                          \

struct StorageMemory_t // not a fan of this being visible
{
	VkDeviceSize size;
	VkDeviceSize offset;
	bool shouldBeTerminated;
};

using StorageMemory = unsigned long long;

template<typename T> class StorageBuffer
{
public:
	StorageBuffer() {}
	~StorageBuffer() { Destroy(); }

	StorageBuffer(const StorageBuffer&) = delete;
	StorageBuffer<T>& operator=(StorageBuffer&&) = delete;

	void Reserve(size_t maxAmountToBeStored, VkBufferUsageFlags usage)
	{
		Vulkan::Context context = Vulkan::GetContext();
		win32::CriticalLockGuard lockGuard(readWriteSection);

		this->logicalDevice = context.logicalDevice;
		this->usage = usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

		reservedSize = maxAmountToBeStored * sizeof(T);

		buffer.Init(reservedSize, this->usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	}

	StorageMemory SubmitNewData(const std::vector<T>& data)
	{
		if (data.empty())
			return 0;

		win32::CriticalLockGuard lockGuard(readWriteSection);
		VkDeviceSize writeSize = sizeof(T) * data.size();

		StorageMemory memoryHandle = 0;
		bool canReuseMemory = FindReusableMemory(memoryHandle, writeSize);						// first check if there any spaces within the buffer that can be filled
		if (canReuseMemory)																		// if a space can be filled then overwrite that space
		{
			memoryData[memoryHandle].size = writeSize;
		}
		else																					// if no spaces could be found then append the new data to the end of the buffer and register the new data
		{
			memoryHandle = nextHandle++;
			memoryData[memoryHandle] = StorageMemory_t{ writeSize, endOfBufferPointer, false };
		}

		CheckForResize(data.size());                                                        // this check should make sure that the next check for an 'out of bounds' error never succeeds, making it redundant

		if (endOfBufferPointer + writeSize > reservedSize && !canReuseMemory)				// throw an error if there is an attempt to write past the buffers capacity
		{
			VkDeviceSize overflow = endOfBufferPointer + writeSize - reservedSize;
			throw VulkanAPIError("Failed to submit new storage buffer data, not enough space has been reserved: " + std::to_string(overflow / sizeof(T)) + " items (" + std::to_string(overflow) + " bytes) of overflow", VK_ERROR_OUT_OF_POOL_MEMORY, __FUNCTION__, __FILENAME__, __LINE__);
		}

		WriteToBuffer(data, memoryHandle);

		if (!canReuseMemory) // the end of the buffer is only moved forward if data is appended
		{
			endOfBufferPointer += writeSize;
			allCreatedMemory.insert(memoryHandle);
		}

		size += data.size();
		hasChanged = true;
		return memoryHandle;
	}

	/// <summary>
	/// Erases all of the data inside the buffer, this also invalidates all memory handles
	/// </summary>
	/// <param name="creationObject"></param>
	void Erase()
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		ClearBuffer();
		allCreatedMemory.clear();
		terminatedMemories.clear();
		memoryData.clear();
		ResetAddressPointer();
		size = 0;
	}

	/// <summary>
	/// Completely erases the given memory from the buffer, replacing the contents with 0
	/// </summary>
	/// <param name="creationObject"></param>
	/// <param name="memory"></param>
	void EraseData(StorageMemory memory)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		CheckHandleValidity(memory, );

		ClearBuffer(memory);
	}

	/// <summary>
	/// Marks the given memory as unused. The contents won't be erased, but can be overwritten. To completely erase data EraseData must be called
	/// </summary>
	/// <param name="memory"></param>
	void DestroyData(StorageMemory memory)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		CheckHandleValidity(memory, );

		StorageMemory_t& memoryInfo = memoryData[memory];
		memoryInfo.shouldBeTerminated = true;
		terminatedMemories.insert(memory);
		size--;
	}
	
	/// <summary>
	/// Gives the distance between the beginning of the buffer and the location of the memory in bytes
	/// </summary>
	/// <param name="memory"></param>
	/// <returns></returns>
	VkDeviceSize GetMemoryOffset(StorageMemory memory)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		CheckHandleValidity(memory, 0);

		return memoryData[memory].offset;
	}

	/// <summary>
	/// Gives the distance between the beginning of the buffer and the location of the memory in the size of the item (offset in bytes / sizeof(item))
	/// </summary>
	/// <param name="memory"></param>
	/// <returns></returns>
	VkDeviceSize GetItemOffset(StorageMemory memory) { return GetMemoryOffset(memory) / sizeof(T); }

	VkDeviceSize GetBufferEnd()    { return (VkDeviceSize)endOfBufferPointer + 1; } // not sure about the + 1
	VkBuffer     GetBufferHandle() { return buffer.Get(); }

	size_t GetSize()    { return size; }
	size_t GetMaxSize() { return reservedSize / sizeof(T); }

	bool HasChanged()   { bool ret = hasChanged; hasChanged = false; return ret; }
	bool HasResized()   { bool ret = hasResized; hasResized = false; return ret; }

	/// <summary>
	/// This resets the internal memory pointer to 0. Any existing data won't be erased, but will be overwritten
	/// </summary>
	void ResetAddressPointer() { size = 0; endOfBufferPointer = 0; }

	void Destroy()
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		buffer.~Buffer();
	}

private:
	void Resize(size_t newSize)
	{
		std::string message = "StorageBuffer resize from " + std::to_string(reservedSize / 1024ull) + " kb to " + std::to_string(newSize / 1024ull) + " kb";
		Console::WriteLine(message, Console::Severity::Debug);

		Buffer newBuffer(newSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

		T* oldPtr = buffer.Map<T>();
		T* newPtr = newBuffer.Map<T>(0, reservedSize); // it is better to copy the buffers via vulkan, but that takes more time to implement

		std::memcpy(newPtr, oldPtr, reservedSize);

		buffer.Unmap();
		newBuffer.Unmap();

		buffer.InheritFrom(newBuffer);

		reservedSize = newSize;
		hasResized = true;
	}

	void CheckForResize(size_t extraSize)
	{
		int multiplier = 2;
		size_t sizeBytes = (size + extraSize) * sizeof(T);

		if (sizeBytes < reservedSize)
			return;

		while (reservedSize * multiplier < sizeBytes)
			multiplier *= 2;

		Resize(reservedSize * multiplier);
	}

	/// <summary>
	/// This looks for for any free space within used memories, reducing the need to create a bigger buffer since terminated spots can be reused
	/// </summary>
	/// <typeparam name="T"></typeparam>
	bool FindReusableMemory(StorageMemory& memory, VkDeviceSize size)
	{
		for (StorageMemory terminatedMemory : terminatedMemories)
		{
			if (memoryData[terminatedMemory].size < size)				// search through all of the terminated memory locations to see if theres enough space in one of them to overwrite without causing overflow
				continue;

			memory = terminatedMemory;
			memoryData[terminatedMemory].shouldBeTerminated = false;	// mark the memory location is no longer being terminated
			terminatedMemories.erase(terminatedMemory);
			return true;
		}
		return false;
	}

	void WriteToBuffer(const std::vector<T>& data, StorageMemory memory)
	{
		StorageMemory_t& memoryInfo = memoryData[memory];

		void* memoryPointer = buffer.Map(memoryInfo.offset, memoryInfo.size);
		memcpy(memoryPointer, data.data(), (size_t)memoryInfo.size);
		buffer.Unmap();
	}

	void ClearBuffer(StorageMemory memory = 0)
	{
		const Vulkan::Context& context = Vulkan::GetContext();
		VkDeviceSize offset = 0;
		VkDeviceSize size = 0;

		if (memory == 0)	// if no specific memory has been given the entire buffer gets cleared
		{
			offset = 0;
			size = reservedSize;
		}
		else				// if a specific memory is given, then only clear that part of the buffer
		{
			StorageMemory_t& memoryInfo = memoryData[memory];
			offset = memoryInfo.offset;
			size = memoryInfo.size;
		}

		VkCommandPool commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);

		VkCommandBuffer commandBuffer = Vulkan::BeginSingleTimeCommands(logicalDevice, commandPool);
		vkCmdFillBuffer(commandBuffer, buffer.Get(), offset, size, 0); // fill the specified part of the buffer with 0's
		Vulkan::EndSingleTimeCommands(logicalDevice, context.graphicsQueue, commandBuffer, commandPool);

		Vulkan::YieldCommandPool(context.graphicsIndex, commandPool);
	}

	bool CheckIfHandleIsValid(StorageMemory memory)
	{
		return allCreatedMemory.count(memory) > 0;
	}

	std::unordered_map<StorageMemory, StorageMemory_t> memoryData;

	std::set<StorageMemory> terminatedMemories;
	std::set<StorageMemory> allCreatedMemory;

	win32::CriticalSection readWriteSection;
	size_t size = 0;

	VkDevice logicalDevice = VK_NULL_HANDLE;
	Buffer buffer;

	StorageMemory nextHandle = 0;

	VkDeviceSize reservedSize = 0;
	VkDeviceSize endOfBufferPointer = 0;
	VkBufferUsageFlags usage = 0;

	bool hasResized = false;
	bool hasChanged = false;
};

#undef CheckHandleValidity