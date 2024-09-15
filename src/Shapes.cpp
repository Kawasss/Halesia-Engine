#include "physics/Shapes.h"
#include "physics/Physics.h"

physx::PxShape* Shape::GetShape()
{
	return shape;
}

Shape Shape::GetShapeFromType(Type type, glm::vec3 extents)
{
	switch (type)
	{
	case Type::Box:
		return Box(extents);
	case Type::Capsule:
		return Capsule(extents);
	case Type::Sphere:
		return Sphere(extents.x);
	}
	return Box(glm::vec3(1));
}

Sphere::Sphere(float radius)
{
	physx::PxSphereGeometry geometry = physx::PxSphereGeometry(radius);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	data = glm::vec3(radius, radius, radius);
	type = Type::Sphere;
}

Box::Box(glm::vec3 extents)
{
	if (extents.y == 0)
		extents.y = 0.1f;
	physx::PxBoxGeometry geometry = physx::PxBoxGeometry(extents.x, extents.y, extents.z);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	data = extents;
	type = Type::Box;
}

Capsule::Capsule(float radius, float halfHeight)
{
	physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radius, halfHeight);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	data = glm::vec3(radius, halfHeight + radius, radius);
	type = Type::Capsule;
}

Capsule::Capsule(glm::vec3 extents)
{
	float radius = extents.x, halfHeight = extents.y - extents.x;
	physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radius, halfHeight);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	data = extents;
	type = Type::Capsule;
}

Plane::Plane()
{
	physx::PxPlaneGeometry geometry = physx::PxPlaneGeometry();
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	type = Type::Plane;
}