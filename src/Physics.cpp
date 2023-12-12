#define PX_PHYSX_STATIC_LIB
#include "physics/Physics.h"
#include <string>
#include <thread>
#include <iostream>
#include "extensions/PxExtensionsAPI.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxDefaultSimulationFilterShader.h"
#include "renderer/Mesh.h"
#include "Object.h"

constexpr float simulationStep = 1 / 60.0f;

Physics* Physics::physics = nullptr;
physx::PxMaterial* Physics::defaultMaterial = nullptr;

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
	sceneInfo.filterShader = physx::PxDefaultSimulationFilterShader;
	sceneInfo.flags = physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	
	scene = physicsObject->createScene(sceneInfo);
}

Physics::~Physics()
{
	physicsObject->release();
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
	}
}

void Physics::AddActor(physx::PxActor& actor)
{
	scene->addActor(actor);
}

void Physics::Simulate(float delta)
{
	timeSinceLastStep += delta;
	if (timeSinceLastStep < simulationStep)
		return;

	timeSinceLastStep -= simulationStep;

	scene->simulate(simulationStep);
	canBeFetched = true;
}

physx::PxActor** Physics::FetchResults(uint32_t& num)
{
	if (!canBeFetched)
		return nullptr;

	scene->fetchResults(true);
	canBeFetched = false;
	return scene->getActiveActors(num);
}