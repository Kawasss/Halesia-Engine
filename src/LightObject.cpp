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
	ret->transform.rotation = light.direction;

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
	transform.rotation = data.lightData.direction;
}

LightGPU LightObject::ToGPUFormat() const
{
	LightGPU ret{};
	ret.pos = glm::vec4(transform.position, outerCutoff);
	ret.color = color;
	ret.direction = glm::vec4(glm::rotate(transform.rotation, glm::vec3(0, 1, -1)), cutoff);
	ret.type = type;

	return ret;
}

void LightObject::DuplicateDataTo(Object* pObject) const
{
	LightObject* pLight = dynamic_cast<LightObject*>(pObject);

	pLight->type = type;
	pLight->color = color;
	pLight->cutoff = cutoff;
	pLight->outerCutoff = outerCutoff;
}