module;

#define PX_PHYSX_STATIC_LIB
#include <PxPhysicsAPI.h>

#include "../core/Object.h"

export module Physics;

import std;

import "../glm.h";

class PhysXErrorHandler : public physx::PxErrorCallback
{
public:
	void reportError(physx::PxErrorCode::Enum errorCode, const char* message, const char* file, int line) override
	{
#ifndef PHYSICS_NO_THROWING
		throw std::runtime_error(std::format("PhysX error: {} in file {} at line {}", message, file, line));
#endif // PHYSICS_NO_THROWING
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

export class Physics
{
public:
	struct RayHitInfo
	{
		glm::vec3 pos;
		glm::vec3 normal;
		glm::vec2 uv;
		float distance;
		Object* object;
	};

	Physics();
	~Physics();
	static void Init();
	static Physics* physics; // really weird way of doing this

	static physx::PxPhysics* GetPhysicsObject() { return physics->physicsObject; }

	static void AddActor(physx::PxActor& actor);
	static void RemoveActor(physx::PxActor& actor);

	static void Simulate(float delta);
	static physx::PxActor** FetchResults(uint32_t& num);
	static void FetchAndUpdateObjects();

	static bool CastRay(glm::vec3 pos, glm::vec3 dir, float maxDistance, RayHitInfo& hitInfo);

	static physx::PxMaterial& GetDefaultMaterial();

	//static physx::PxTriangleMesh* CreateTriangleMesh(const Mesh& mesh);

private:
	PhysXErrorHandler* errorHandler{};
	PhysicsOnContactCallback* contactCallback{};
	physx::PxDefaultAllocator* allocator{};

	physx::PxFoundation* foundation = nullptr;
	physx::PxPhysics* physicsObject = nullptr;
	physx::PxScene* scene = nullptr;

	bool canBeFetched = false;
	float timeSinceLastStep = 0;
};