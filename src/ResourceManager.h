#pragma once
#include <unordered_map>
#include <stdint.h>
#include <unordered_set>
#include "renderer/Vulkan.h"

// don't know how good it is to put this into a seperate file

typedef uint64_t Handle;
typedef Handle ApeironMemory;

struct ApeironMemory_t // not a fan of this being visible
{
	VkDeviceSize size;
	VkDeviceSize offset;
	bool shouldBeTerminated;
};

namespace ResourceManager // add mutexes for secure operations
{
	Handle GenerateHandle();
}

template<typename T> class ApeironBuffer
{
public:
	ApeironBuffer() {}
	void Reserve(const VulkanCreationObject& creationObject, size_t maxAmountToBeStored, VkBufferUsageFlags usage)
	{
		this->logicalDevice = creationObject.logicalDevice;
		this->usage = usage;

		reservedBufferSize = maxAmountToBeStored * sizeof(T);

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		// create an empty buffer with the specified size
		Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, reservedBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

		Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, reservedBufferSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, buffer, deviceMemory);
		Vulkan::CopyBuffer(creationObject.logicalDevice, commandPool, creationObject.queue, stagingBuffer, buffer, reservedBufferSize);

		vkDestroyBuffer(creationObject.logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(creationObject.logicalDevice, stagingBufferMemory, nullptr);

		Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);
	}

	ApeironMemory SubmitNewData(std::vector<T> data)
	{
		VkDeviceSize writeSize = sizeof(T) * data.size();

		ApeironMemory memoryHandle = 0;
		bool canReuseMemory = FindReusableMemory(memoryHandle, writeSize);						// first check if there any spaces within the buffer that can be filled
		if (canReuseMemory)																		// if a space can be filled then overwrite that space
		{
			memoryData[memoryHandle].size = writeSize;
		}
		else																					// if no spaces could be found then append the new data to the end of the buffer and register the new data
		{
			memoryHandle = ResourceManager::GenerateHandle();
			memoryData[memoryHandle] = ApeironMemory_t{ writeSize, endOfBufferPointer, false };
		}

		if (endOfBufferPointer + writeSize > reservedBufferSize && !canReuseMemory)				// throw an error if there is an attempt write over the buffers capacity
		{
			VkDeviceSize overflow = endOfBufferPointer + writeSize - reservedBufferSize;
			throw VulkanAPIError("Failed to submit new Apeiron buffer data, not enough space has been reserved: " + std::to_string(overflow / sizeof(T)) + " items (" + std::to_string(overflow) + " bytes) of overflow", VK_ERROR_OUT_OF_POOL_MEMORY, nameof(SubmitNewData), __FILENAME__, __STRLINE__);
		}

		WriteToBuffer(data, memoryHandle);

		if (!canReuseMemory) // the end of the buffer is only moved forward if data is appended
			endOfBufferPointer += writeSize;

		hasChanged = true;
		return memoryHandle;
	}

	void Clear(const VulkanCreationObject& creationObject)
	{
		Destroy(); // destroy the buffer and reset the pointer to the end, then recreates the buffer
		endOfBufferPointer = 0;

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, reservedBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		VkCommandPool commandPool = Vulkan::FetchNewCommandPool(creationObject);

		Vulkan::CreateBuffer(creationObject.logicalDevice, creationObject.physicalDevice, reservedBufferSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, buffer, deviceMemory);
		Vulkan::CopyBuffer(creationObject.logicalDevice, commandPool, creationObject.queue, stagingBuffer, buffer, reservedBufferSize);

		vkDestroyBuffer(creationObject.logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(creationObject.logicalDevice, stagingBufferMemory, nullptr);

		Vulkan::YieldCommandPool(creationObject.queueIndex, commandPool);
	}

	/// <summary>
	/// Marks the given memory as unused. "ReorderBuffer" must be called in order for the destruction to take effect 
	/// </summary>
	/// <param name="memory"></param>
	void DestroyData(ApeironMemory memory)
	{
		ApeironMemory_t& memoryInfo = memoryData[memory];
		memoryInfo.shouldBeTerminated = true;
	}

	VkDeviceSize GetMemoryOffset(ApeironMemory memory) { return memoryData[memory].offset; }
	VkDeviceSize GetBufferEnd() { return (VkDeviceSize)endOfBufferPointer + 1; } // not sure about the + 1
	VkBuffer GetBufferHandle() { return buffer; }
	bool HasChanged() { bool ret = hasChanged; hasChanged = false; return ret; }

	/// <summary>
	/// This resets the internal memory pointer to 0. Any existing data won't be erased, but will be overwritten
	/// </summary>
	void ResetAddressPointer() { endOfBufferPointer = 0; }

	void Destroy()
	{
		vkDestroyBuffer(logicalDevice, buffer, nullptr);
		vkFreeMemory(logicalDevice, deviceMemory, nullptr);
	}

private:
	std::unordered_map<ApeironMemory, ApeironMemory_t> memoryData;
	std::unordered_set<ApeironMemory> terminatedMemories;

	/// <summary>
	/// This looks for for any free space within used memories, reducing the need to create a bigger buffer since terminated spots can be reused
	/// </summary>
	/// <typeparam name="T"></typeparam>
	bool FindReusableMemory(ApeironMemory& memory, VkDeviceSize size)
	{
		for (ApeironMemory terminatedMemory : terminatedMemories)
		{
			if (memoryData[terminatedMemory].size >= size)					// search through all of the terminated memory locations to see if theres enough space in one of them to overwrite without causing overflow
			{
				memory = terminatedMemory;
				memoryData[terminatedMemory].shouldBeTerminated = false;	// mark the memory location is no longer being terminated
				terminatedMemories.erase(terminatedMemory);
				return true;
			}
		}
		return false;
	}

	void WriteToBuffer(const std::vector<T>& data, ApeironMemory memory)
	{
		ApeironMemory_t& memoryInfo = memoryData[memory];

		void* memoryPointer;
		vkMapMemory(logicalDevice, deviceMemory, memoryInfo.offset, memoryInfo.size, 0, &memoryPointer);
		memcpy(memoryPointer, data.data(), (size_t)memoryInfo.size);
		vkUnmapMemory(logicalDevice, deviceMemory);
	}

	VkDevice logicalDevice =      VK_NULL_HANDLE;
	VkBuffer buffer =             VK_NULL_HANDLE;
	VkDeviceMemory deviceMemory = VK_NULL_HANDLE;

	VkDeviceSize reservedBufferSize = 0;
	VkDeviceSize endOfBufferPointer = 0;
	VkBufferUsageFlags usage =        0;

	bool hasChanged = false;
};