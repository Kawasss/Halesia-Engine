#pragma once
#include <string>

#include <sol.hpp>

class Transform;

class Script
{
public:
	Script(const std::string& file);

	void Start();
	void Update(float delta);
	void Destroy();

	void SetTransform(const Transform& transform);
	void GetTransform(Transform& transform);

	void Reset(const std::string& file);

private:
	void PerformSetup();

	sol::state state;
};