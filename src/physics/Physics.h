#pragma once
#include <PxPhysicsAPI.h>

#include <string>

#include "../glm.h"

struct Mesh;
class Object;

class PhysXErrorHandler;
class PhysicsOnContactCallback;

namespace physx
{
	class PxDefaultAllocator;
	class PxFoundation;
	class PxPhysics;
	class PxScene;
}

class Physics
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
	PhysXErrorHandler*         errorHandler{};
	PhysicsOnContactCallback*  contactCallback{};
	physx::PxDefaultAllocator* allocator{};
	
	physx::PxFoundation* foundation = nullptr;
	physx::PxPhysics* physicsObject = nullptr;
	physx::PxScene* scene = nullptr;

	bool canBeFetched = false;
	float timeSinceLastStep = 0;
};