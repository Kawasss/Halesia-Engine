#include "physics/Shapes.h"
#include "physics/Physics.h"

physx::PxShape* Shape::GetShape()
{
	return shape;
}

Sphere::Sphere(float radius)
{
	physx::PxSphereGeometry geometry = physx::PxSphereGeometry(radius);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	type = SHAPE_TYPE_SPHERE;
}

Box::Box(glm::vec3 extents)
{
	if (extents.y == 0)
		extents.y = 0.1f;
	physx::PxBoxGeometry geometry = physx::PxBoxGeometry(extents.x, extents.y, extents.z);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	type = SHAPE_TYPE_BOX;
}

Capsule::Capsule(float radius, float halfHeight)
{
	physx::PxCapsuleGeometry geometry = physx::PxCapsuleGeometry(radius, halfHeight);
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	type = SHAPE_TYPE_CAPSULE;
}

Plane::Plane()
{
	physx::PxPlaneGeometry geometry = physx::PxPlaneGeometry();
	shape = Physics::GetPhysicsObject()->createShape(geometry, *Physics::defaultMaterial);
	type = SHAPE_TYPE_PLANE;
}