#pragma once
#include "../glm.h"

struct Light
{
	enum class Type : int
	{
		Directional = 0,
		Point = 1,
		Spot = 2,
	};

	glm::vec3 pos;
	glm::vec3 color;
	Type type;
};

