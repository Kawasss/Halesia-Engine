#pragma once
#include "Object.h"

#include "../renderer/Light.h"

class LightObject : public Object
{
public:
	static LightObject* Create(const ObjectCreationData& data);
	static LightObject* Create(const Light& light);
	static LightObject* Create();

	Light::Type type;

	float cutoff = .0f;
	float outerCutoff = .0f;

	glm::vec3 color = glm::vec3(1.0f);
	glm::vec3 direction = glm::vec3(0.0f);

	LightGPU ToGPUFormat() const;

private:
	LightObject();
	void Init(const ObjectCreationData& data);
};