#pragma once
#include "scripting/Script.h"

#include "core/Transform.h"
#include "core/Object.h"

Script::Script(const std::string& file, Object* pOwner)
{
	assert(pOwner != nullptr);
	this->pOwner = pOwner;

	PerformSetup();

	state.script_file(file);
}

void Script::Start()
{
	SetTransform(pOwner->transform);

	state["Start"]();

	GetTransform(pOwner->transform);
}

void Script::Update(float delta)
{
	SetTransform(pOwner->transform);

	state["Update"](delta);

	GetTransform(pOwner->transform);
}

void Script::Destroy()
{
	SetTransform(pOwner->transform);

	state["Destroy"]();

	GetTransform(pOwner->transform);
}

template<typename T>
static void WriteVec3(const T& table, glm::vec3 value)
{
	table["x"] = value.x;
	table["y"] = value.y;
	table["z"] = value.z;
}

template<typename T>
static glm::vec3 ReadVec3(const T& table)
{
	return glm::vec3(table["x"], table["y"], table["z"]);
}

template<typename T>
static void WriteQuat(const T& table, glm::quat value)
{
	table["x"] = value.x;
	table["y"] = value.y;
	table["z"] = value.z;
	table["w"] = value.w;
}

template<typename T>
static glm::quat ReadQuat(const T& table)
{
	return glm::quat(table["w"], table["x"], table["y"], table["z"]);
}

void Script::SetTransform(const Transform& transform)
{
	state["transform"] = GetTransformAsTable();
}

void Script::GetTransform(Transform& transform)
{
	const auto& luaTransform = state["transform"];

	transform.position = ReadVec3(luaTransform["position"]);
	transform.scale    = ReadVec3(luaTransform["scale"]);
	transform.rotation = ReadQuat(luaTransform["rotation"]);
}

void Script::Reset(const std::string& file)
{
	if (!file.empty())
	{
		state.~state(); // i dont know a better way to reset this without making it a pointer
		new(&state) sol::state();
	}

	PerformSetup();

	state.script_file(file);
}

void Script::PerformSetup()
{
	state.open_libraries();
	state.script("package.path = 'scripts/?.lua'");
	state.script("local Transform = require 'Transform'\ntransform = Transform:new()");
}

sol::table Script::GetTransformAsTable()
{
	auto vecNew = state["vec3"]["new"];
	auto quatNew = state["Quaternion"]["new"];

	auto position = vecNew(pOwner->transform.position.x, pOwner->transform.position.y, pOwner->transform.position.z);
	auto scale = vecNew(pOwner->transform.scale.x, pOwner->transform.scale.y, pOwner->transform.scale.z);
	auto rotation = quatNew(pOwner->transform.rotation.x, pOwner->transform.rotation.y, pOwner->transform.rotation.z);

	auto table = state["Transform"]["new"](position, rotation, scale);

	return table;
}