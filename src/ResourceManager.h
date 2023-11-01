#pragma once
#include <unordered_map>
#include <stdint.h>
#include <unordered_set>
#include "renderer/Vulkan.h"

// don't know how good it is to put this into a seperate file

typedef uint64_t Handle;

namespace ResourceManager // add mutexes for secure operations
{
	Handle GenerateHandle();
}

