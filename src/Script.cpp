#pragma once
#include "scripting/Script.h"

#include "core/Transform.h"
#include "core/Object.h"
#include "core/Console.h"

Script::Script(const std::string& code, Object* pOwner)
{
	assert(pOwner != nullptr);
	this->pOwner = pOwner;

	PerformSetup();

	state.script(code);
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
static float EnsureValueValidity(const T& table, const std::string_view& name)
{
	if (table.valid())
	{
		return static_cast<float>(table);
	}
	else
	{
		Console::WriteLine("table '{}' is nil", Console::Severity::Error, name);
		return 0.0f;
	}
}

template<typename T>
static void WriteVec3(const T& table, const glm::vec3& value)
{
	table["x"] = value.x;
	table["y"] = value.y;
	table["z"] = value.z;
}

template<typename T>
static glm::vec3 ReadVec3(const T& table)
{
	return glm::vec3(EnsureValueValidity(table["x"], "vec3.x"), EnsureValueValidity(table["y"], "vec3.y"), EnsureValueValidity(table["z"], "vec3.z"));
}

template<typename T>
static void WriteQuat(const T& table, const glm::quat& value)
{
	table["x"] = value.x;
	table["y"] = value.y;
	table["z"] = value.z;
	table["w"] = value.w;
}

template<typename T>
static glm::quat ReadQuat(const T& table)
{
	return glm::quat(EnsureValueValidity(table["w"], "Quaternion.w"), EnsureValueValidity(table["x"], "Quaternion.x"), EnsureValueValidity(table["y"], "Quaternion.y"), EnsureValueValidity(table["z"], "Quaternion.z"));
}

void Script::SetTransform(const Transform& transform)
{
	WriteTransformToState(transform);
}

void Script::GetTransform(Transform& transform)
{
	const auto& luaTransform = state["transform"];

	transform.position = ReadVec3(luaTransform["position"]);
	transform.scale    = ReadVec3(luaTransform["scale"]);
	transform.rotation = ReadQuat(luaTransform["rotation"]);
}

void Script::Reset(const std::string& code)
{
	if (!code.empty())
	{
		state.~state(); // i dont know a better way to reset this without making it a pointer
		new(&state) sol::state();
	}

	PerformSetup();

	state.script(code);
}

void Script::PerformSetup()
{
	state.open_libraries();
	state.script("package.path = 'scripts/?.lua'");
	state.script("Transform = require 'Transform'\ntransform = Transform.new()");
}

void Script::WriteTransformToState(const Transform& transform)
{
	auto trans = state["transform"];
	if (!trans.valid())
		return;

	auto vec3New = state["vec3"]["new"];

	trans["position"] = vec3New(transform.position.x, transform.position.y, transform.position.z);
	trans["scale"] = vec3New(transform.scale.x, transform.scale.y, transform.scale.z);
	trans["rotation"] = state["Quaternion"]["new"](transform.rotation.w, transform.rotation.x, transform.rotation.y, transform.rotation.z);
}