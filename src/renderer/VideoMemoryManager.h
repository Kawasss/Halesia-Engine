#pragma once
#include <vulkan/vulkan.h>

#include <vector>

namespace vvm
{
	template<typename VulkanType>
	using Deleter = void(*)(VulkanType);

	template<typename VulkanType, Deleter<VulkanType> deleter>
	class Handle
	{
	public:
		using ValueType = VulkanType;
		
		static constexpr Deleter<VulkanType> destructor = deleter;

		Handle() = default;
		Handle(VulkanType other) : handle(other) {}

		virtual ~Handle() {}

		void Destroy()
		{
			if (!IsValid())
				return;

			deleter(handle);
			Invalidate();
		}

		VulkanType& Get() { return handle; }
		const VulkanType& Get() const { return handle; }

		bool IsValid() const { return handle != VK_NULL_HANDLE; }
		void Invalidate() { handle = VK_NULL_HANDLE; }

	protected:
		VulkanType handle = VK_NULL_HANDLE;
	};

	template<typename VulkanType, Deleter<VulkanType> deleter>
	class SmartHandle : public Handle<VulkanType, deleter>
	{
	public:
		using HandleType = Handle<VulkanType, deleter>;
		using SmartType  = SmartHandle<VulkanType, deleter>;

		SmartHandle() = default;
		SmartHandle(const SmartHandle&) = delete;

		SmartHandle& operator=(HandleType&& other) noexcept
		{
			this->handle = other.Get();
			other.Invalidate();

			return *this;
		}

		SmartHandle& operator=(SmartType&& other) noexcept
		{
			this->handle = other.Get();
			other.Invalidate();

			return *this;
		}

		~SmartHandle()
		{
			this->Destroy();
		}
	};
	
	struct Segment;
	struct MemoryBlock;
	struct MemoryCore;

	extern void Destroy(VkImage image);
	extern void Destroy(VkBuffer buffer);

	using Image  = Handle<VkImage,  vvm::Destroy>;
	using Buffer = Handle<VkBuffer, vvm::Destroy>;

	using SmartImage  = SmartHandle<Image::ValueType,  Image::destructor>;
	using SmartBuffer = SmartHandle<Buffer::ValueType, Buffer::destructor>;

	extern Image  AllocateImage(VkImage image, VkMemoryPropertyFlags properties);
	extern Buffer AllocateBuffer(VkBuffer buffer, VkMemoryPropertyFlags properties, void* pNext = nullptr);

	extern void* MapBuffer(Buffer buffer, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE, VkMemoryMapFlags flags = 0);
	extern void UnmapBuffer(Buffer buffer);

	extern void ForceDestroy();

	extern void Init();
	extern void ShutDown(); // completely shuts down the manager, rendering it unusable until it is initialized again

	struct DbgSegment
	{
		VkDeviceSize begin;
		VkDeviceSize end;
	};

	struct DbgMemoryBlock
	{
		uint64_t flags;
		VkDeviceSize size;
		VkDeviceSize alignment;
		VkDeviceSize used;
		std::vector<DbgSegment> segments;
	};

	extern std::vector<DbgMemoryBlock> DbgGetMemoryBlocks();
};