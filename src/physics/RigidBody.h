#pragma once
#include "Physics.h"
#include "Shapes.h"
#include "../glm.h"

enum RigidBodyType
{
	RIGID_BODY_NONE,
	RIGID_BODY_STATIC,
	RIGID_BODY_DYNAMIC
};
inline extern std::string RigidBodyTypeToString(RigidBodyType type);

class RigidBody
{
public:
	RigidBody() {}
	RigidBody(Shape shape, RigidBodyType type, glm::vec3 pos = glm::vec3(0), glm::vec3 rot = glm::vec3(0));
	void MovePosition(glm::vec3 pos, glm::vec3 rot);
	void SetScale(glm::vec3 scale);
	void SetUserData(void* data);

	glm::vec3 GetPosition();
	glm::vec3 GetRotation();

	RigidBodyType type = RIGID_BODY_NONE;
	Shape shape;

private:
	physx::PxTransform GetTransform(); // dont know a better way to deal with the different types of rigids

	physx::PxRigidDynamic* rigidDynamic = nullptr; // a rigidbody can have either of these types
	physx::PxRigidStatic*  rigidStatic = nullptr;
};