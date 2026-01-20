module;

#include <Windows.h>

#include "io/CreationData.h"

#include "renderer/Light.h"

#include "glm.h"

module Core.LightObject;

import std;

import IO.BinaryStream;

import Core.Object;

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
	ret->lType = light.type;
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
	lType = data.lightData.type;
	color = data.lightData.color;
	cutoff = data.lightData.cutoff;
	outerCutoff = data.lightData.outerCutoff;
}

LightGPU LightObject::ToGPUFormat() const
{
	LightGPU ret{};
	ret.pos = glm::vec4(transform.position, outerCutoff);
	ret.color = color;
	ret.direction = glm::vec4(glm::rotate(transform.rotation, glm::vec3(0, 1, -1)), cutoff);
	ret.type = lType;

	return ret;
}

void LightObject::DuplicateDataTo(Object* pObject) const
{
	LightObject* pLight = dynamic_cast<LightObject*>(pObject);

	pLight->lType = lType;
	pLight->color = color;
	pLight->cutoff = cutoff;
	pLight->outerCutoff = outerCutoff;
}

void LightObject::SerializeSelf(BinaryStream& stream) const
{
	stream << static_cast<std::underlying_type_t<Light::Type>>(lType);
	stream << cutoff << outerCutoff << color.x << color.y << color.z;
}

void LightObject::DeserializeSelf(const BinarySpan& stream)
{
	std::underlying_type_t<Light::Type> intermediary = 0;
	stream >> intermediary;

	lType = static_cast<Light::Type>(intermediary);
	
	stream >> cutoff >> outerCutoff >> color.x >> color.y >> color.z;
}