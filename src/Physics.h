#pragma once
#include "PhysXErrorHandler.h"
#include "PxPhysicsAPI.h"

class Physics
{
public:
	Physics();
	~Physics();

private:
	PhysXErrorHandler errorHandler;
	physx::PxDefaultAllocator allocator;
	physx::PxFoundation* foundation;
	physx::PxPhysics* physicsObject;
};