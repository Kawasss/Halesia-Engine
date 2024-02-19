#define PX_PHYSX_STATIC_LIB
#include <string>
#include <thread>
#include <iostream>

#include "physics/Physics.h"

#include "extensions/PxExtensionsAPI.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxDefaultSimulationFilterShader.h"

#include "renderer/Mesh.h"
#include "core/Object.h"

constexpr float simulationStep = 1 / 60.0f;

Physics* Physics::physics = nullptr;
physx::PxMaterial* Physics::defaultMaterial = nullptr;

inline glm::vec3 PxToGlm(physx::PxVec3 vec)
{
	return { vec.x, vec.y, vec.z };
}

void PhysicsOnContactCallback::onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs)
{
	for (uint32_t i = 0; i < nbPairs; i++)
	{
		const physx::PxContactPair& pair = pairs[i];
		Object* firstObj = static_cast<Object*>(pairHeader.actors[0]->userData);
		Object* secondObj = static_cast<Object*>(pairHeader.actors[1]->userData);

		if (pairs->events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND)
		{
			firstObj->OnCollisionEnter();
			secondObj->OnCollisionEnter();
		}
		else if (pairs->events & physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS)
		{
			firstObj->OnCollisionStay();
			secondObj->OnCollisionStay();
		}
		else if (pairs->events & physx::PxPairFlag::eNOTIFY_TOUCH_LOST)
		{
			firstObj->OnCollisionExit();
			secondObj->OnCollisionExit();
		}
	}
}

void Physics::Init()
{
	physics = new Physics();
	defaultMaterial = physics->physicsObject->createMaterial(0.4f, 0.4f, 0.2f);
}

Physics::Physics()
{
	foundation = PxCreateFoundation(PX_PHYSICS_VERSION, allocator, errorHandler);
	if (!foundation)
		throw std::runtime_error("Failed to create a PhysX foundation object");

	bool recordAllocations = true;

	physicsObject = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, physx::PxTolerancesScale(), recordAllocations);
	if (!physicsObject)
		throw std::runtime_error("Failed to create a PxPhysics object");

	std::cout << "Successfully created a PhysX context with version " << PX_PHYSICS_VERSION_MAJOR << "." << PX_PHYSICS_VERSION_MINOR << "\n";

	if (!PxInitExtensions(*physicsObject, nullptr))
		throw std::runtime_error("Failed to init the PhysX extensions");

	int numberOfThreads = std::thread::hardware_concurrency();
	dispatcher = physx::PxDefaultCpuDispatcherCreate(numberOfThreads);
	
	physx::PxSceneDesc sceneInfo{ physicsObject->getTolerancesScale() };
	sceneInfo.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
	sceneInfo.cpuDispatcher = dispatcher;
	sceneInfo.filterShader = FilterShader;
	sceneInfo.flags = physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	sceneInfo.simulationEventCallback = &contactCallback;
	sceneInfo.kineKineFilteringMode = physx::PxPairFilteringMode::eKEEP;
	sceneInfo.staticKineFilteringMode = physx::PxPairFilteringMode::eKEEP;
	
	scene = physicsObject->createScene(sceneInfo);
}

physx::PxFilterFlags Physics::FilterShader(physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1, physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
{
	if (physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1))
	{
		pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
		return physx::PxFilterFlag::eDEFAULT;
	}

	pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;

	//if ((filterData0.word0 & filterData1.word1) && (filterData1.word0 & filterData0.word1))
		pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;

	return physx::PxFilterFlag::eDEFAULT;
}

Physics::~Physics()
{
	physicsObject->release();
}

bool Physics::CastRay(glm::vec3 pos, glm::vec3 dir, float maxDistance, RayHitInfo& info)
{
	physx::PxRaycastBuffer hit;
	bool hasHit = physics->scene->raycast({ pos.x, pos.y, pos.z }, { dir.x, dir.y, dir.z }, maxDistance, hit);
	if (!hasHit)
		return false;

	info.pos = PxToGlm(hit.block.position);
	info.uv = { hit.block.u, hit.block.v };
	info.normal = PxToGlm(hit.block.normal);
	info.distance = hit.block.distance;
	if (hit.block.actor != nullptr)
		info.object = static_cast<Object*>(hit.block.actor->userData);

	return hasHit;
}

void Physics::FetchAndUpdateObjects()
{
	uint32_t numActors;
	physx::PxActor** actors = FetchResults(numActors);
	if (actors == nullptr)
		return;
	
	for (int i = 0; i < numActors; i++)
	{
		physx::PxActor* actor = actors[i];
		Object* object = static_cast<Object*>(actor->userData);
		
		physx::PxTransform trans = actor->is<physx::PxRigidActor>()->getGlobalPose();
		glm::quat quat = glm::quat(trans.q.w, trans.q.x, trans.q.y, trans.q.z);

		object->transform.position = glm::vec3(trans.p.x, trans.p.y, trans.p.z);
		object->transform.rotation = glm::degrees(glm::eulerAngles(quat));
		object->rigid.SetForce();
	}
}

void Physics::AddActor(physx::PxActor& actor)
{
	if (!physics->scene->addActor(actor))
		throw std::runtime_error("Failed to add an actor");
}

void Physics::RemoveActor(physx::PxActor& actor)
{
	physics->scene->removeActor(actor);
}

void Physics::Simulate(float delta)
{
	physics->timeSinceLastStep += delta;
	if (physics->timeSinceLastStep < simulationStep)
		return;

	physics->timeSinceLastStep -= simulationStep;

	if (!physics->scene->simulate(simulationStep))
		throw std::runtime_error("cannot simulate physics");
	physics->canBeFetched = true;
}

physx::PxActor** Physics::FetchResults(uint32_t& num)
{
	if (!physics->canBeFetched)
		return nullptr;

	if (!physics->scene->fetchResults(true))
		throw std::runtime_error("cannot fetch physics");
	physics->canBeFetched = false;
	return physics->scene->getActiveActors(num);
}