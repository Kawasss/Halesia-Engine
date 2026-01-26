module;

#include "../glm"

export module Renderer.Light;

import std;

export struct Light
{
	enum class Type : int
	{
		Directional = 0,
		Point = 1,
		Spot = 2,
	};
	static Type StringToType(const std::string_view& str);
	static std::string_view TypeToString(Type type);

	std::string name;

	float cutoff = .0f;
	float outerCutoff = .0f;

	glm::vec3 pos;
	glm::vec3 color = glm::vec3(1.0f);
	glm::vec3 direction;
	Type type;
};

#pragma pack(push, 1)
struct LightGPU // the struct thats to be uploaded to the GPU
{
	static LightGPU CreateFromLight(const Light& l)
	{
		return { {l.pos, l.outerCutoff }, l.color, { l.direction, l.cutoff }, l.type };
	}

	alignas(16) glm::vec4 pos;
	alignas(16) glm::vec3 color = glm::vec3(1.0f);
	alignas(16) glm::vec4 direction; // only for spot lights (kinda bad to use so much space for 1 light type ??) the cutoff is placed inside direction.w, the outer cutoff is placed inside pos.w
	alignas(16) Light::Type type;
};
#pragma pack(pop)