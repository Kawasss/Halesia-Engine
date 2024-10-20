#include "renderer/GarbageManager.h"

struct DestroyQueue
{
public:
	static constexpr int WAIT_TIME = 3; // in frames (3 frames should be enough to garantuee that the object is no longer used

private:
	
};

namespace vgm
{
	void DeleteObject(VkObjectType type, uint64_t handle)
	{

	}

	void CollectGarbage()
	{

	}

	void ForceDelete()
	{
		
	}
}