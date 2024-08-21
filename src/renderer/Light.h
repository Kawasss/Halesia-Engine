#pragma once
#include "../glm.h"

#pragma pack(push, 1)
struct Light
{
	enum class Type : int
	{
		Directional = 0,
		Point = 1,
		Spot = 2,
	};

	alignas(16) glm::vec4 pos;
	alignas(16) glm::vec3 color = glm::vec3(1.0f);
	alignas(16) glm::vec4 direction; // only for spot lights (kinda bad to use so much space for 1 light type ??) the cutoff is placed inside direction.w, the outer cutoff is placed inside pos.w
	alignas(16) Type type;
};
#pragma pack(pop)