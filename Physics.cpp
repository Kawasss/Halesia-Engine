#define PX_PHYSX_STATIC_LIB
#include "Physics.h"
#include <extensions/PxExtensionsAPI.h>

using namespace physx;

Physics::Physics()
{
	foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorHandler);
	if (!foundation)
		throw std::runtime_error("Failed to create a PhysX foundation object");

	bool recordAllocations = true;

	physicsObject = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, PxTolerancesScale(), recordAllocations);
	if (!physicsObject)
		throw std::runtime_error("Failed to create a PxPhysics object");
}

Physics::~Physics()
{
	physicsObject->release();
}