#pragma once
#include <PxPhysicsAPI.h>
#include <PxShape.h>

#include <iostream>
#include <string>

#include "../glm.h"

struct Mesh;
class Object;

struct RayHitInfo
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
	float distance;
	Object* object;
};

class PhysXErrorHandler : public physx::PxErrorCallback
{
public:
	void reportError(physx::PxErrorCode::Enum errorCode, const char* message, const char* file, int line) override
	{
		std::string error = "PhysX error: " + (std::string)message + " in file " + (std::string)file + " at line " + std::to_string(line);
		std::cout << error << std::endl;
		#ifndef PHYSICS_NO_THROWING
		throw std::runtime_error(error);
		#endif
	}
};

class PhysicsOnContactCallback : public physx::PxSimulationEventCallback
{
	void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) override;
	void onConstraintBreak(physx::PxConstraintInfo* constraints, physx::PxU32 count) override {}
	void onWake(physx::PxActor** actors, physx::PxU32 count) override {}
	void onSleep(physx::PxActor** actors, physx::PxU32 count) override {}
	void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override {}
	void onAdvance(const physx::PxRigidBody* const* bodyBuffer, const physx::PxTransform* poseBuffer, const physx::PxU32 count) override {}

};

class Physics
{
public:
	Physics();
	~Physics();
	static void Init();
	static Physics* physics; // really weird way of doing this
	static physx::PxMaterial* defaultMaterial;
	physx::PxDefaultCpuDispatcher* dispatcher = nullptr;

	static physx::PxPhysics* GetPhysicsObject() { return physics->physicsObject; }
	static void AddActor(physx::PxActor& actor);
	static void RemoveActor(physx::PxActor& actor);
	static void Simulate(float delta);
	static physx::PxActor** FetchResults(uint32_t& num);
	static void FetchAndUpdateObjects();
	static bool CastRay(glm::vec3 pos, glm::vec3 dir, float maxDistance, RayHitInfo& hitInfo);
	static physx::PxTriangleMesh* CreateTriangleMesh(const Mesh& mesh);

private:
	static physx::PxFilterFlags FilterShader(physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1, physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize);

	PhysXErrorHandler errorHandler{};
	PhysicsOnContactCallback contactCallback{};
	physx::PxDefaultAllocator allocator{};
	
	physx::PxFoundation* foundation = nullptr;
	physx::PxPhysics* physicsObject = nullptr;
	physx::PxScene* scene = nullptr;

	bool canBeFetched = false;
	float timeSinceLastStep = 0;
};