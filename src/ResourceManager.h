#pragma once
#include <unordered_map>
#include <unordered_set>
#include <stdint.h>
#include "glm.h"

// don't know how good it is to put this into a seperate file

typedef uint64_t Handle;

namespace ResourceManager // add mutexes for secure operations
{
	Handle GenerateHandle();
	glm::vec3 ConvertHandleToVec3(Handle handle);
}

