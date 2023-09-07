#define PX_PHYSX_STATIC_LIB
#include "Physics.h"
#include <string>
#include <iostream>
#include <extensions/PxExtensionsAPI.h>

Physics::Physics()
{
	foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorHandler);
	if (!foundation)
		throw std::runtime_error("Failed to create a PhysX foundation object");

	bool recordAllocations = true;

	physicsObject = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, physx::PxTolerancesScale(), recordAllocations);
	if (!physicsObject)
		throw std::runtime_error("Failed to create a PxPhysics object");
}

Physics::~Physics()
{
	physicsObject->release();
}