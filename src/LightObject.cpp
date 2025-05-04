#include <Windows.h>

#include "core/LightObject.h"

#include "io/CreationData.h"

LightObject::LightObject() : Object(InheritType::Light)
{

}

LightObject* LightObject::Create(const ObjectCreationData& data)
{
	LightObject* ret = new LightObject();
	ret->Init(data);

	return ret;
}

LightObject* LightObject::Create(const Light& light)
{
	LightObject* ret = new LightObject();

	ret->transform.position = light.pos;
	ret->type = light.type;
	ret->color = light.color;
	ret->cutoff = light.cutoff;
	ret->outerCutoff = light.outerCutoff;
	ret->direction = light.direction;

	return ret;
}

LightObject* LightObject::Create()
{
	return new LightObject();
}

void LightObject::Init(const ObjectCreationData& data)
{
	Initialize(data);

	transform.position = data.lightData.pos;
	type = data.lightData.type;
	color = data.lightData.color;
	cutoff = data.lightData.cutoff;
	outerCutoff = data.lightData.outerCutoff;
	direction = data.lightData.direction;
}

LightGPU LightObject::ToGPUFormat() const
{
	LightGPU ret{};
	ret.pos = glm::vec4(transform.position, 1.0f);
	ret.color = color;
	ret.direction = glm::vec4(direction, 1.0f);
	ret.type = type;

	return ret;
}