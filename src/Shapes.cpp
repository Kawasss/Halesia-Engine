#include "physics/Shapes.h"
#include "physics/Physics.h"

physx::PxShape* Shape::GetShape()
{
	return shape;
}

Shape Shape::GetShapeFromType(ShapeType type, glm::vec3 extents)
{
	switch (type)
	{
	case SHAPE_TYPE_BOX:
		return Box(extents);
	case SHAPE_TYPE_CAPSULE:
		return Capsule(extents);
	case SHAPE_TYPE_SPHERE:
		return Sphere(extents.x);
	}
	return Box(glm::vec3(1));
}

Sphere::Sphere(float radius)
{
	physx::PxSphereGeometry geometry = physx::PxSphereGeometry(radius);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	data = glm::vec3(radius, radius, radius);
	type = SHAPE_TYPE_SPHERE;
}

Box::Box(glm::vec3 extents)
{
	if (extents.y == 0)
		extents.y = 0.1f;
	physx::PxBoxGeometry geometry = physx::PxBoxGeometry(extents.x, extents.y, extents.z);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	data = extents;
	type = SHAPE_TYPE_BOX;
}

Capsule::Capsule(float radius, float halfHeight)
{
	physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radius, halfHeight);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	data = glm::vec3(radius, halfHeight + radius, radius);
	type = SHAPE_TYPE_CAPSULE;
}

Capsule::Capsule(glm::vec3 extents)
{
	float radius = extents.x, halfHeight = extents.y - extents.x;
	physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radius, halfHeight);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	data = extents;
	type = SHAPE_TYPE_CAPSULE;
}

Plane::Plane()
{
	physx::PxPlaneGeometry geometry = physx::PxPlaneGeometry();
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	type = SHAPE_TYPE_PLANE;
}

std::string ShapeTypeToString(ShapeType type)
{
	switch (type)
	{
	case SHAPE_TYPE_BOX:
		return "SHAPE_TYPE_BOX";
	case SHAPE_TYPE_CAPSULE:
		return "SHAPE_TYPE_CAPSULE";
	case SHAPE_TYPE_NONE:
		return "SHAPE_TYPE_NONE";
	case SHAPE_TYPE_PLANE:
		return "SHAPE_TYPE_PLANE";
	case SHAPE_TYPE_SPHERE:
		return "SHAPE_TYPE_SPHERE";
	}
	return "";
}