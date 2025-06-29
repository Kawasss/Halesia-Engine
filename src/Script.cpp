#pragma once
#include "scripting/Script.h"

#include "core/Transform.h"

Script::Script(const std::string& file)
{
	PerformSetup();

	state.script_file(file);
}

void Script::Start()
{
	state["Start"]();
}

void Script::Update(float delta)
{
	state["Update"](delta);
}

void Script::Destroy()
{
	state["Destroy"]();
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
	const auto& luaTransform = state["transform"];

	WriteVec3(luaTransform["position"], transform.position);
	WriteVec3(luaTransform["scale"],    transform.scale);
	WriteQuat(luaTransform["rotation"], transform.rotation);
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
	state.~state(); // i dont know a better way to reset this without making it a pointer
	new(&state) sol::state();

	PerformSetup();

	state.script_file(file);
}

void Script::PerformSetup()
{
	state.open_libraries();
	state.script("package.path = 'scripts/?.lua'");
	state.script("local Transform = require 'Transform'\ntransform = Transform:new()");
}