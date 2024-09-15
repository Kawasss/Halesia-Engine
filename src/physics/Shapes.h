#pragma once
#include "../glm.h"

namespace physx
{
	class PxShape;
}

class Shape
{
public:
	enum class Type : uint8_t
	{
		None,
		Sphere,
		Box,
		Capsule,
		Plane,
	};
	static std::string TypeToString(Type type);

	static Shape GetShapeFromType(Type type, glm::vec3 extents);
	physx::PxShape* GetShape();
	Type type;
	glm::vec3 data; // optional data that a shape can use as it wants

protected:
	physx::PxShape* shape;
};

class Sphere : public Shape
{
public:
	Sphere(float radius);
};

class Box : public Shape
{
public:
	Box(glm::vec3 extents);
};

class Capsule : public Shape
{
public:
	/// <summary>
	/// Creates a capsule shape from its extents
	/// </summary>
	/// <param name="extents">
	/// The radius of the capsule is taken from the x axis of the extents.
	/// The half height of the capsule is calculated by subtracting the radius (x axis) from the y axis
	/// </param>
	Capsule(glm::vec3 extents);
	Capsule(float radius, float halfHeight);
};

class Plane : public Shape
{
public:
	Plane();
};