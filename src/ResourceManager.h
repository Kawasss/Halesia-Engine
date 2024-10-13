#pragma once
#include "glm.h"

// don't know how good it is to put this into a seperate file

using Handle = unsigned long long;

namespace ResourceManager // add mutexes for secure operations
{
	Handle GenerateHandle();
	glm::vec3 ConvertHandleToVec3(Handle handle);
}

