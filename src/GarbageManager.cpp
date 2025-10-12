#include <vector>

#include "renderer/GarbageManager.h"
#include "renderer/Vulkan.h"

#include "system/CriticalSection.h"

namespace vgm
{
	win32::CriticalSection criticalSection;

	struct DeleteInfo
	{
		DeleteInfo() = default;
		DeleteInfo(VkObjectType type, uint64_t handle) : type(type), handle(handle) {}

		VkObjectType type;
		uint64_t handle;
	};

	struct DestroyQueue
	{
	public:
		static constexpr int WAIT_TIME = 3; // in frames (3 frames should be enough to garantuee that the object is no longer used

		void Update() 
		{
			lastFrame = frame;
			frame = (frame + 1) % WAIT_TIME; 
		}

		void Add(VkObjectType type, uint64_t handle) 
		{ 
			queues[lastFrame].emplace_back(type, handle);
		}

		std::vector<DeleteInfo>& GetCurrentQueue() { return queues[frame]; }

	private:
		int frame = 0, lastFrame = 2;
		std::vector<DeleteInfo> queues[WAIT_TIME];
	};
	DestroyQueue queue;

	void DeleteObject(VkObjectType type, uint64_t handle)
	{
		win32::CriticalLockGuard guard(criticalSection);
		queue.Add(type, handle);
	}

#define DELETE_HANDLE(func, type) func(device, reinterpret_cast<type>(info.handle), nullptr); break
	void DestroyType(const DeleteInfo& info)
	{
		if (info.handle == 0)
			return;

		VkDevice device = Vulkan::GetContext().logicalDevice;
		switch (info.type)
		{
		case VK_OBJECT_TYPE_IMAGE:      DELETE_HANDLE(vkDestroyImage, VkImage);
		case VK_OBJECT_TYPE_IMAGE_VIEW: DELETE_HANDLE(vkDestroyImageView, VkImageView);

		case VK_OBJECT_TYPE_BUFFER:        DELETE_HANDLE(vkDestroyBuffer, VkBuffer);
		case VK_OBJECT_TYPE_DEVICE_MEMORY: DELETE_HANDLE(vkFreeMemory, VkDeviceMemory);

		case VK_OBJECT_TYPE_PIPELINE:        DELETE_HANDLE(vkDestroyPipeline, VkPipeline);
		case VK_OBJECT_TYPE_PIPELINE_LAYOUT: DELETE_HANDLE(vkDestroyPipelineLayout, VkPipelineLayout);

		case VK_OBJECT_TYPE_DESCRIPTOR_POOL:       DELETE_HANDLE(vkDestroyDescriptorPool, VkDescriptorPool);
		case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT: DELETE_HANDLE(vkDestroyDescriptorSetLayout, VkDescriptorSetLayout);

		case VK_OBJECT_TYPE_FRAMEBUFFER: DELETE_HANDLE(vkDestroyFramebuffer, VkFramebuffer);
		case VK_OBJECT_TYPE_RENDER_PASS: DELETE_HANDLE(vkDestroyRenderPass, VkRenderPass);

		case VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR: DELETE_HANDLE(vkDestroyAccelerationStructureKHR, VkAccelerationStructureKHR);

		case VK_OBJECT_TYPE_COMMAND_POOL: DELETE_HANDLE(vkDestroyCommandPool, VkCommandPool);

		case VK_OBJECT_TYPE_SWAPCHAIN_KHR: DELETE_HANDLE(vkDestroySwapchainKHR, VkSwapchainKHR);
		}
	}
#undef DELETE_HANDLE

	void CollectGarbage()
	{
		win32::CriticalLockGuard guard(criticalSection);

		std::vector<DeleteInfo>& infos = queue.GetCurrentQueue();

		for (const DeleteInfo& info : infos)
			DestroyType(info);

		infos.clear();
		queue.Update();
 	}

	void ForceDelete()
	{
		for (int i = 0; i < DestroyQueue::WAIT_TIME; i++)
			CollectGarbage();
	}
}