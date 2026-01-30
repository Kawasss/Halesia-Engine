module;

#include <Windows.h>
#include <vulkan/vulkan.h>

#include "../system/CriticalSection.h"

#include "../core/Console.h"

export module Renderer.StorageBuffer;

import std;

import Renderer.ResizableBuffer;
import Renderer.Vulkan;

#define CheckHandleValidity(memory, ret)                                                                                                             \
if (!CheckIfHandleIsValid(memory))                                                                                                                   \
{                                                                                                                                                    \
	Console::WriteLine("An invalid memory handle ({}) has been found in {}", Console::Severity::Error, static_cast<uint64_t>(memory), __FUNCTION__); \
    /*__debugbreak();*/                                                                                                                                  \
	return ret;                                                                                                                                      \
}                                                                                                                                                    \

export template<typename T>
class StorageBuffer
{
public:
	using Memory = std::uintptr_t;

	StorageBuffer() = default;
	~StorageBuffer() { Destroy(); }

	StorageBuffer(const StorageBuffer&) = delete;
	StorageBuffer<T>& operator=(StorageBuffer&&) = delete;

	void Reserve(size_t maxAmountToBeStored, VkBufferUsageFlags usage)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		buffer.Init(maxAmountToBeStored * sizeof(T), ResizableBuffer::MemoryType::Gpu, usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	}

	/// <summary>
	/// Appends the given data at the end of the buffer, if the buffer is not big enough it may resize.
	/// </summary>
	/// <param name="data">data to append</param>
	/// <returns></returns>
	Memory SubmitNewData(const std::span<const T>& data)
	{
		if (data.empty())
			return 0;

		win32::CriticalLockGuard lockGuard(readWriteSection);
		VkDeviceSize writeSize = sizeof(T) * data.size();

		Memory memoryHandle = FindReusableMemory(writeSize);	// first check if there any spaces within the buffer that can be filled
		bool canReuseMemory = memoryHandle != INVALID_HANDLE;

		if (canReuseMemory)											// if a space can be filled then overwrite that space
		{
			memoryData[memoryHandle].size = writeSize;
		}
		else														// if no spaces could be found then append the new data to the end of the buffer and register the new data
		{
			memoryHandle = nextHandle++;
			memoryData.emplace(std::piecewise_construct, std::forward_as_tuple(memoryHandle), std::forward_as_tuple(writeSize, endOfBufferPointer, false));
		}

		WriteToBuffer(data, memoryHandle);

		if (!canReuseMemory) // the end of the buffer is only moved forward if data is appended
		{
			endOfBufferPointer += writeSize;
			allCreatedMemory.insert(memoryHandle);
		}

		activeMemories.insert(memoryHandle);

		size += data.size();
		hasChanged = true;
		return memoryHandle;
	}

	/// <summary>
	/// Invalidates all existing handles and resets the address pointer. The data in the buffer will not be reset to 0
	/// </summary>
	void Reset()
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		ClearBuffer();
		allCreatedMemory.clear();
		activeMemories.clear();
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
	void EraseData(Memory memory)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		CheckHandleValidity(memory, );

		ClearBuffer(memory);
		activeMemories.erase(memory);
	}

	/// <summary>
	/// Marks the given memory as unused. The contents won't be erased, but can be overwritten. To completely erase data EraseData must be called
	/// </summary>
	/// <param name="memory"></param>
	void DestroyData(Memory memory)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		CheckHandleValidity(memory, );

		StorageMemory_t& memoryInfo = memoryData[memory];
		memoryInfo.shouldBeTerminated = true;
		size -= memoryInfo.size / sizeof(T);

		terminatedMemories.insert(memory);
		activeMemories.erase(memory);
		memoryData.erase(memory);
	}

	/// <summary>
	/// Gives the distance between the beginning of the buffer and the location of the memory in bytes
	/// </summary>
	/// <param name="memory"></param>
	/// <returns></returns>
	VkDeviceSize GetMemoryOffset(Memory memory)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		CheckHandleValidity(memory, 0);

		return memoryData.at(memory).offset;
	}

	VkDeviceSize GetMemoryCount(Memory memory)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		CheckHandleValidity(memory, 0);

		return memoryData.at(memory).size;
	}

	VkDeviceAddress GetDeviceAddressOffset(Memory memory)
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);
		CheckHandleValidity(memory, 0);

		if (!(buffer.GetUsageFlags() & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))
			return 0;

		return Vulkan::GetDeviceAddress(GetBufferHandle()) + GetMemoryOffset(memory);
	}

	static constexpr Memory INVALID_HANDLE = 0;

	/// <summary>
	/// Gives the distance between the beginning of the buffer and the location of the memory in the size of the item (offset in bytes / sizeof(item))
	/// </summary>
	/// <param name="memory"></param>
	/// <returns></returns>
	VkDeviceSize GetItemOffset(Memory memory) 
	{ 
		return GetMemoryOffset(memory) / sizeof(T); 
	}

	VkDeviceSize GetItemCount(Memory memory)
	{
		return GetMemoryCount(memory) / sizeof(T);
	}

	VkDeviceSize GetBufferEnd() { return (VkDeviceSize)endOfBufferPointer + 1; } // not sure about the + 1
	VkBuffer     GetBufferHandle() { return buffer.Get(); }

	size_t GetSize() { return size; }
	size_t GetMaxSize() { return buffer.GetSize() / sizeof(T); }

	bool HasChanged() { bool ret = hasChanged; hasChanged = false; return ret; }
	bool HasResized() { return buffer.Resized(); }

	/// <summary>
	/// This resets the internal memory pointer to 0. Any existing data won't be erased, but will be overwritten
	/// </summary>
	void ResetAddressPointer() { size = 0; endOfBufferPointer = 0; }

	void Destroy()
	{
		win32::CriticalLockGuard lockGuard(readWriteSection);

		if (!buffer.IsValid())
			return;

		if (!activeMemories.empty())
		{
			Console::WriteLine("There are {} active handles at time of destruction", Console::Severity::Error, activeMemories.size());
		}

		buffer.~ResizableBuffer();
	}

private:
	struct StorageMemory_t
	{
		StorageMemory_t() = default;
		StorageMemory_t(VkDeviceSize s, VkDeviceSize o, bool sh) : size(s), offset(o), shouldBeTerminated(sh) {}

		VkDeviceSize size;
		VkDeviceSize offset;
		bool shouldBeTerminated;
	};

	/// <summary>
	/// This looks for for any free space within used memories, reducing the need to create a bigger buffer since terminated spots can be reused
	/// </summary>
	/// <typeparam name="T"></typeparam>
	Memory FindReusableMemory(VkDeviceSize size)
	{
		for (Memory terminatedMemory : terminatedMemories)
		{
			if (memoryData[terminatedMemory].size < size)				// search through all of the terminated memory locations to see if theres enough space in one of them to overwrite without causing overflow
				continue;

			memoryData[terminatedMemory].shouldBeTerminated = false;	// mark the memory location is no longer being terminated
			terminatedMemories.erase(terminatedMemory);
			return terminatedMemory;
		}
		return INVALID_HANDLE;
	}

	void WriteToBuffer(const std::span<const T>& data, Memory memory)
	{
		buffer.Write(data, memoryData[memory].offset);
	}

	void ClearBuffer(Memory memory = INVALID_HANDLE)
	{
		VkDeviceSize offset = 0;
		VkDeviceSize size = 0;

		if (memory == INVALID_HANDLE)	// if no specific memory has been given the entire buffer gets cleared
		{
			offset = 0;
			size = buffer.GetSize();
		}
		else				            // if a specific memory is given, then only clear that part of the buffer
		{
			StorageMemory_t& memoryInfo = memoryData[memory];
			offset = memoryInfo.offset;
			size = memoryInfo.size;
		}

		//buffer.Fill(0, size, offset);
	}

	bool CheckIfHandleIsValid(Memory memory)
	{
		return allCreatedMemory.contains(memory);
	}

	std::map<Memory, StorageMemory_t> memoryData;

	std::set<Memory> terminatedMemories;
	std::set<Memory> allCreatedMemory;
	std::set<Memory> activeMemories;

	win32::CriticalSection readWriteSection;
	size_t size = 0;

	ResizableBuffer buffer;

	Memory nextHandle = 1;

	VkDeviceSize endOfBufferPointer = 0;

	bool hasChanged = false;
};

#undef CheckHandleValidity