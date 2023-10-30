#pragma once
#include <unordered_map>
#include <stdint.h>
#include "renderer/Vulkan.h"

// don't know how good it is to put this into a seperate file

typedef uint64_t Handle;
typedef Handle ApeironMemory;

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
		if (lastWriteOffset + writeSize > reservedBufferSize)
		{
			uint32_t overflow = lastWriteOffset + writeSize - reservedBufferSize;
			throw VulkanAPIError("Failed to submit new Apeiron buffer data, not enough space has been reserved: " + std::to_string(overflow / sizeof(T)) + " items (" + std::to_string(overflow) + " bytes) of overflow", VK_ERROR_OUT_OF_POOL_MEMORY, nameof(SubmitNewData), __FILENAME__, __STRLINE__);
		}

		void* memoryPointer;
		vkMapMemory(logicalDevice, deviceMemory, lastWriteOffset, writeSize, 0, &memoryPointer);
		memcpy(memoryPointer, data.data(), (size_t)writeSize);
		vkUnmapMemory(logicalDevice, deviceMemory);
		
		ApeironMemory memoryHandle = ResourceManager::GenerateHandle();
		memoryData[memoryHandle] = ApeironMemory_t{ writeSize, lastWriteOffset, false };

		lastWriteOffset += writeSize;
		hasChanged = true;
		return memoryHandle;
	}

	void Clear(const VulkanCreationObject& creationObject)
	{
		Destroy();
		lastWriteOffset = 0;

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
		// maybe add something here that marks a memory location for termination when reordering. this removes the need to overwrite the buffer with empty data.
	}

	/// <summary>
	/// Reorders the buffer to remove any empty spaces inbetween the sub-buffers.
	/// This should be called after removing data.
	/// </summary>
	void ReorderBuffer() // maybe do something with receiving a handle to a struct(?) which contains the offset and size of the buffer to delete
	{
		for (const ApeironMemory_t& memoryInfo : memoryData)
		{
			if (memoryInfo.shouldBeTerminated)
			{
				// move all upcoming data backwards to fill the unused space
			}
		}
	}

	VkDeviceSize GetMemoryOffset(ApeironMemory memory) { return memoryData[memory].offset; }
	VkDeviceSize GetBufferEnd() { return (VkDeviceSize)lastWriteOffset + 1; } // not sure about the + 1
	VkBuffer GetBufferHandle() { return buffer; }
	bool HasChanged() { bool ret = hasChanged; hasChanged = false; return ret; }

	/// <summary>
	/// This resets the internal memory pointer to 0. Any existing data won't be erased, but will be overwritten
	/// </summary>
	void ResetAddressPointer() { lastWriteOffset = 0; }

	void Destroy()
	{
		vkDestroyBuffer(logicalDevice, buffer, nullptr);
		vkFreeMemory(logicalDevice, deviceMemory, nullptr);
	}

private:
	struct ApeironMemory_t
	{
		VkDeviceSize size;
		uint32_t offset;
		bool shouldBeTerminated;
	};
	
	std::unordered_map<ApeironMemory, ApeironMemory_t> memoryData;

	VkDevice logicalDevice =      VK_NULL_HANDLE;
	VkBuffer buffer =             VK_NULL_HANDLE;
	VkDeviceMemory deviceMemory = VK_NULL_HANDLE;

	VkDeviceSize reservedBufferSize = 0;
	uint32_t lastWriteOffset =        0;
	VkBufferUsageFlags usage =        0;

	bool hasChanged = false;
};