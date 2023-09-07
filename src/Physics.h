#pragma once
#include "PxPhysicsAPI.h"
#include <iostream>
#include <string>

class PhysXErrorHandler : public physx::PxErrorCallback
{
public:
	void reportError(physx::PxErrorCode::Enum errorCode, const char* message, const char* file, int line) override
	{
		std::string error = "PhysX error: " + (std::string)message + " in file " + (std::string)file + " at line " + std::to_string(line);
		std::cout << error << std::endl;
		throw std::runtime_error(error);
	}
};

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