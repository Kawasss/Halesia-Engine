module;

#include "../core/Object.h"
#include "../core/Transform.h"

export module Scripting.Script;

import std;

import <sol.hpp>;

export class Script
{
public:
	Script() = default;
	Script(const std::string& code, Object* pOwner);

	void Start();
	void Update(float delta);
	void Destroy();

	void Reset(const std::string& code);

private:
	void WriteTransformToState(const Transform& transform);

	void SetTransform(const Transform& transform);
	void GetTransform(Transform& transform);

	void PerformSetup();

	Object* pOwner = nullptr; // this pointer is garantueed safe by the constructor

	sol::state state;
};