#include "physics/RigidBody.h"
#include "core/Transform.h"
#include "core/Object.h"

RigidBody::RigidBody(Shape shape, RigidBodyType type, glm::vec3 pos, glm::vec3 rot)
{
	this->shape = shape;
	this->type = type;
	glm::quat rotation = glm::quat(glm::radians(rot));
	physx::PxTransform transform = physx::PxTransform(pos.x, pos.y, pos.z, physx::PxQuat(rotation.x, rotation.y, rotation.z, rotation.w));

	switch (type)
	{
	case RIGID_BODY_DYNAMIC:
		rigidDynamic = physx::PxCreateDynamic(*Physics::GetPhysicsObject(), transform, *shape.GetShape(), 1);
		Physics::AddActor(*rigidDynamic);
		break;
	case RIGID_BODY_STATIC:
		rigidStatic = physx::PxCreateStatic(*Physics::GetPhysicsObject(), transform, *shape.GetShape());
		Physics::AddActor(*rigidStatic);
		break;
	case RIGID_BODY_KINEMATIC:
		rigidDynamic = physx::PxCreateDynamic(*Physics::GetPhysicsObject(), transform, *shape.GetShape(), 1);
		Physics::AddActor(*rigidDynamic);
		rigidDynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		break;
	case RIGID_BODY_NONE:
		throw std::invalid_argument("Failed to create a rigidbody: invalid rigidbody type argument (RIGID_BODY_NONE)");
	}
}

void RigidBody::MovePosition(Transform& transform) // only works if the rigid is dynamic !!
{
	rigidDynamic->setKinematicTarget(GetTransform(transform));
}

void RigidBody::ForcePosition(Transform& transform)
{
	physx::PxTransform trans = GetTransform(transform);
	rigidDynamic == nullptr ? rigidStatic->setGlobalPose(trans) : rigidDynamic->setGlobalPose(trans);
}

void RigidBody::ChangeShape(Shape& shape)
{
	if (rigidStatic == nullptr)
	{
		rigidDynamic->detachShape(*this->shape.GetShape());
		this->shape = shape;
		rigidDynamic->attachShape(*shape.GetShape());
	}
	else
	{
		rigidStatic->detachShape(*this->shape.GetShape());
		this->shape = shape;
		rigidStatic->attachShape(*shape.GetShape());
	}
}

void RigidBody::Destroy()
{
	physx::PxActor* actor = rigidDynamic == nullptr ? rigidStatic->is<physx::PxActor>() : rigidDynamic->is<physx::PxActor>();
	Physics::RemoveActor(*actor);
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

void RigidBody::AddForce(glm::vec3 force)
{
	if (rigidDynamic == nullptr)
		return;
	queuedUpForce += force;
}

void RigidBody::SetForce()
{
	if (rigidDynamic == nullptr || queuedUpForce == glm::vec3(0))
		return;
	rigidDynamic->addForce({ queuedUpForce.x, queuedUpForce.y, queuedUpForce.z });
	queuedUpForce = glm::vec3(0);
}

void RigidBody::SetUserData(void* data)
{
	if (rigidStatic == nullptr)
		rigidDynamic->userData = data;
	else
		rigidStatic->userData = data;
}

void* RigidBody::GetUserData()
{
	if (rigidStatic == nullptr)
		return rigidDynamic->userData;
	return rigidStatic->userData;
}

physx::PxTransform RigidBody::GetTransform(Transform& transform)
{
	glm::quat quat = glm::quat(glm::radians(transform.rotation));
	return physx::PxTransform(transform.position.x, transform.position.y, transform.position.z, physx::PxQuat(quat.x, quat.y, quat.z, quat.w));
}

physx::PxTransform RigidBody::GetTransform()
{
	return rigidStatic == nullptr ? rigidDynamic->getGlobalPose() : rigidStatic->getGlobalPose();
}