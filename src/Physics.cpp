#define PX_PHYSX_STATIC_LIB
#include "Physics.h"
#include <string>
#include <thread>
#include <iostream>
#include "extensions/PxExtensionsAPI.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxDefaultSimulationFilterShader.h"
#include "renderer/Mesh.h"

Physics* Physics::physics = nullptr;

void Physics::Init()
{
	physics = new Physics();
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
	
	scene = physicsObject->createScene(sceneInfo);
}

Physics::~Physics()
{
	physicsObject->release();
}

physx::PxShape* Physics::CreatePhysicsObject(Mesh& mesh)
{
	float x = (mesh.max.x - mesh.min.x) / 2;
	float y = (mesh.max.y - mesh.min.y) / 2;
	float z = (mesh.max.z - mesh.min.z) / 2;

	physx::PxMaterial* material = physicsObject->createMaterial(1, 1, 1);
	physx::PxShape* shape = physicsObject->createShape(physx::PxBoxGeometry(x, y, z), *material, false, physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE | physx::PxShapeFlag::eSIMULATION_SHAPE);
	
	physx::PxRigidDynamic* rigidDynamic = physx::PxCreateDynamic(*physicsObject, physx::PxTransform(0, 0, 0), *shape, 1);

	scene->addActor(*rigidDynamic);

	return shape;
}

void Physics::Simulate(float delta)
{
	
}