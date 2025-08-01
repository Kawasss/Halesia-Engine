#pragma once
#include <string>

#include <sol.hpp>

class Transform;
class Object;

class Script
{
public:
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