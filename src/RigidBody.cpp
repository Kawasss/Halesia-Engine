#include "physics/RigidBody.h"
#include "Transform.h"

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

void RigidBody::MovePosition(Transform& transform) // only works if the rigid is dynamic !!
{
	physx::PxTransform trans = GetTransform(transform);
	rigidDynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
	rigidDynamic->setKinematicTarget(trans);
}

void RigidBody::ForcePosition(Transform& transform)
{
	physx::PxTransform trans = GetTransform(transform);
	rigidDynamic == nullptr ? rigidStatic->setGlobalPose(trans) : rigidDynamic->setGlobalPose(trans);
}

glm::vec3 RigidBody::GetPosition()
{
	physx::PxTransform trans = GetTransform();
	return glm::vec3(trans.p.x, trans.p.y, trans.p.z);
}

glm::vec3 RigidBody::GetRotation()
{
	physx::PxTransform trans = GetTransform();
	return glm::eulerAngles(glm::quat(trans.q.w, trans.q.x, trans.q.y, trans.q.z));
}

void RigidBody::SetUserData(void* data)
{
	if (rigidStatic == nullptr)
		rigidDynamic->userData = data;
	else
		rigidStatic->userData = data;
}

physx::PxTransform RigidBody::GetTransform(Transform& transform)
{
	glm::quat quat = glm::quat(transform.rotation);
	return physx::PxTransform(transform.position.x, transform.position.y, transform.position.z, physx::PxQuat(quat.x, quat.y, quat.z, quat.w));
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