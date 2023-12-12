#pragma once
#include "../glm.h"

enum ShapeType
{
	SHAPE_TYPE_NONE,
	SHAPE_TYPE_SPHERE,
	SHAPE_TYPE_BOX,
	SHAPE_TYPE_CAPSULE,
	SHAPE_TYPE_PLANE
};

namespace physx
{
	class PxShape;
}

class Shape
{
public:
	physx::PxShape* GetShape();
	ShapeType type;

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
	Capsule(float radius, float halfHeight);
};

class Plane : public Shape
{
public:
	Plane();
};