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

	alignas(16) glm::vec3 pos;
	alignas(16) glm::vec3 color = glm::vec3(1.0f);
	alignas(16) glm::vec3 data; // the data can vary between light types
	alignas(16) Type type;
};
#pragma pack(pop)