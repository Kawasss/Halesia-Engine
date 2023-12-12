#include "physics/RigidBody.h"

RigidBody::RigidBody(Shape shape, RigidBodyType type, glm::vec3 pos, glm::vec3 rot)
{
	this->shape = shape;
	this->type = type;
	glm::quat rotation = glm::quat(rot);
	physx::PxTransform transform = physx::PxTransform(pos.x, pos.y, pos.z, physx::PxQuat(rotation.x, rotation.y, rotation.z, rotation.w));

	switch (type)
	{
	case RIGID_BODY_DYNAMIC:
		rigidDynamic = physx::PxCreateDynamic(*Physics::GetPhysicsObject(), transform, *shape.GetShape(), 1);
		Physics::physics->AddActor(*rigidDynamic);
		break;
	case RIGID_BODY_STATIC:
		rigidStatic = physx::PxCreateStatic(*Physics::GetPhysicsObject(), transform, *shape.GetShape());
		Physics::physics->AddActor(*rigidStatic);
		break;
	case RIGID_BODY_NONE:
		throw std::invalid_argument("Failed to create a rigidbody: invalid rigidbody type argument (RIGID_BODY_NONE)");
	}
}

void RigidBody::MovePosition(glm::vec3 pos, glm::vec3 rot) // doesnt work the right way
{
	glm::quat quat = glm::quat(rot);
	physx::PxTransform trans = physx::PxTransform(pos.x, pos.y, pos.z, physx::PxQuat(quat.x, quat.y, quat.z, quat.w));
	/*
	if (rigidDynamic == nullptr)
		throw std::runtime_error("Cannot make a static rigid body kinematic");
	rigidDynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
	rigidDynamic->setKinematicTarget(trans);
	*/
	// THIS IS NOT THE CORRECT SOLUTION, JUST A TEMP FIX

	rigidStatic == nullptr ? rigidDynamic->setGlobalPose(trans) : rigidStatic->setGlobalPose(trans);
}

glm::vec3 RigidBody::GetPosition()
{
	physx::PxTransform trans = GetTransform();
	return glm::vec3(trans.p.x, trans.p.y, trans.p.z);
}

glm::vec3 RigidBody::GetRotation()
{
	physx::PxTransform trans = GetTransform();
	return glm::vec3(1) * glm::quat(trans.q.w, trans.q.x, trans.q.y, trans.q.z); // not sure
}

void RigidBody::SetUserData(void* data)
{
	if (rigidStatic == nullptr)
		rigidDynamic->userData = data;
	else
		rigidStatic->userData = data;
}

physx::PxTransform RigidBody::GetTransform()
{
	return rigidStatic == nullptr ? rigidDynamic->getGlobalPose() : rigidStatic->getGlobalPose();
}

std::string RigidBodyTypeToString(RigidBodyType type)
{
	switch (type)
	{
	case RIGID_BODY_DYNAMIC:
		return "RIGID_BODY_DYNAMIC";
	case RIGID_BODY_STATIC:
		return "RIGID_BODY_STATIC";
	case RIGID_BODY_NONE:
		return "RIGID_BODY_NONE";
	}
	return "";
}