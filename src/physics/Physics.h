#pragma once
#include "PxPhysicsAPI.h"
#include "PxShape.h"
#include <iostream>
#include <string>

struct Mesh;

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
	static void Init();
	static Physics* physics; // really weird way of doing this
	static physx::PxMaterial* defaultMaterial;

	static physx::PxPhysics* GetPhysicsObject() { return physics->physicsObject; }
	void AddActor(physx::PxActor& actor);
	void RemoveActor(physx::PxActor& actor);
	void Simulate(float delta);
	physx::PxActor** FetchResults(uint32_t& num);
	void FetchAndUpdateObjects();

private:
	PhysXErrorHandler errorHandler{};
	physx::PxDefaultAllocator allocator{};
	physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
	physx::PxFoundation* foundation = nullptr;
	physx::PxPhysics* physicsObject = nullptr;
	physx::PxScene* scene = nullptr;

	bool canBeFetched = false;
	float timeSinceLastStep = 0;
};