#pragma once
#include <vulkan/vulkan.h>

namespace vgm // vulkan garbage manager
{
#define DECLARE_DELETOR(handleType, objectType) template<> void Delete<##handleType##>(##handleType handle) { DeleteObject(##objectType##, reinterpret_cast<uint64_t>(handle)); }

	template<typename T> void Delete(T handle)
	{
		//static_assert(false, "Cannot delete the given vulkan object");
	}

	//DECLARE_DELETOR(VkBuffer, VK_OBJECT_TYPE_BUFFER)

	void DeleteObject(VkObjectType type, uint64_t handle);

	void CollectGarbage();
	void ForceDelete();

#undef DECLARE_DELETOR
}