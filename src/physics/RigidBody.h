#pragma once
#include "Physics.h"
#include "Shapes.h"
#include "../glm.h"

class Transform;

class RigidBody
{
public:
	enum class Type : uint8_t
	{
		None,
		Static,
		Dynamic,
		Kinematic,
	};
	static std::string TypeToString(Type type);

	RigidBody() {}
	RigidBody(Shape shape, Type type, glm::vec3 pos = glm::vec3(0), glm::vec3 rot = glm::vec3(0));
	~RigidBody();
	void Destroy();

	void MovePosition(Transform& transform);
	void ForcePosition(Transform& transform);
	void ChangeShape(Shape& shape);
	void SetScale(glm::vec3 scale);
	void AddForce(glm::vec3 force);
	void SetForce();
	void SetUserData(void* data);
	void ChangeType(Type type);
	void* GetUserData();

	glm::vec3 GetPosition();
	glm::vec3 GetRotation();

	Type type = Type::None;
	Shape shape;
	glm::vec3 queuedUpForce = glm::vec3(0);

private:
	physx::PxTransform GetTransform(); // dont know a better way to deal with the different types of rigids
	physx::PxTransform GetTransform(Transform& transform);

	physx::PxRigidDynamic* rigidDynamic = nullptr; // a rigidbody can have either of these types
	physx::PxRigidStatic*  rigidStatic = nullptr;
	physx::PxRigidBody* rigid = nullptr;
};