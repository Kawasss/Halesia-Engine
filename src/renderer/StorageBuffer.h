#pragma once
#include "Vulkan.h"
#include "Buffer.h"
#include "../ResourceManager.h"
#include "../core/Console.h"
#include <iostream>
#include <mutex>

#define CheckHandleValidity(memory, ret)                                                                                                            \
if (!CheckIfHandleIsValid(memory))                                                                                                                  \
{                                                                                                                                                   \
	Console::WriteLine("An invalid memory handle (" + ToHexadecimalString(memory) + ") has been found in " + __FUNCTION__, MESSAGE_SEVERITY_ERROR); \
	return ret;                                                                                                                                     \
}                                                                                                                                                   \


struct StorageMemory_t // not a fan of this being visible
{
	VkDeviceSize size;
	VkDeviceSize offset;
	bool shouldBeTerminated;
};

typedef Handle StorageMemory;

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
		std::lock_guard<std::mutex> lockGuard(readWriteMutex);
		this->logicalDevice = context.logicalDevice;
		this->usage = usage;

		reservedBufferSize = maxAmountToBeStored * sizeof(T);

		// create an empty buffer with the specified size
		Buffer stagingBuffer(reservedBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		VkCommandPool commandPool = Vulkan::FetchNewCommandPool(context.graphicsIndex);

		buffer.Init(reservedBufferSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
		Vulkan::CopyBuffer(commandPool, context.graphicsQueue, stagingBuffer.Get(), buffer.Get(), reservedBufferSize);

		Vulkan::YieldCommandPool(context.graphicsIndex, commandPool);
	}

	StorageMemory SubmitNewData(const std::vector<T>& data)
	{
		std::lock_guard<std::mutex> lockGuard(readWriteMutex);
		VkDeviceSize writeSize = sizeof(T) * data.size();

		StorageMemory memoryHandle = 0;
		bool canReuseMemory = FindReusableMemory(memoryHandle, writeSize);						// first check if there any spaces within the buffer that can be filled
		if (canReuseMemory)																		// if a space can be filled then overwrite that space
		{
			memoryData[memoryHandle].size = writeSize;
		}
		else																					// if no spaces could be found then append the new data to the end of the buffer and register the new data
		{
			memoryHandle = ResourceManager::GenerateHandle();
			memoryData[memoryHandle] = StorageMemory_t{ writeSize, endOfBufferPointer, false };
		}

		if (endOfBufferPointer + writeSize > reservedBufferSize && !canReuseMemory)				// throw an error if there is an attempt to write past the buffers capacity
		{
			VkDeviceSize overflow = endOfBufferPointer + writeSize - reservedBufferSize;
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
		std::lock_guard<std::mutex> lockGuard(readWriteMutex);
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
		std::lock_guard<std::mutex> lockGuard(readWriteMutex);
		CheckHandleValidity(memory, );

		ClearBuffer(memory);
	}

	/// <summary>
	/// Marks the given memory as unused. The contents won't be erased, but can be overwritten. To completely erase data EraseData must be called
	/// </summary>
	/// <param name="memory"></param>
	void DestroyData(StorageMemory memory)
	{
		std::lock_guard<std::mutex> lockGuard(readWriteMutex);
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
		std::lock_guard<std::mutex> lockGuard(readWriteMutex);
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
	size_t GetMaxSize() { return reservedBufferSize / sizeof(T); }

	bool HasChanged()   { bool ret = hasChanged; hasChanged = false; return ret; }

	/// <summary>
	/// This resets the internal memory pointer to 0. Any existing data won't be erased, but will be overwritten
	/// </summary>
	void ResetAddressPointer() { size = 0; endOfBufferPointer = 0; }

	void Destroy()
	{
		std::lock_guard<std::mutex> lockGuard(readWriteMutex);
		buffer.~Buffer();
	}

private:
	std::unordered_map<StorageMemory, StorageMemory_t> memoryData;
	std::unordered_set<StorageMemory> terminatedMemories;
	std::unordered_set<StorageMemory> allCreatedMemory;
	std::mutex readWriteMutex;
	size_t size = 0;

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
			size = reservedBufferSize;
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

	VkDevice logicalDevice = VK_NULL_HANDLE;
	Buffer buffer;

	VkDeviceSize reservedBufferSize = 0;
	VkDeviceSize endOfBufferPointer = 0;
	VkBufferUsageFlags usage = 0;

	bool hasChanged = false;
};

#undef CheckHandleValidity