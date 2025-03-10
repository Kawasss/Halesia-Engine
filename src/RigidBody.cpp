#include "physics/RigidBody.h"
#include "core/Transform.h"
#include "core/Object.h"

RigidBody::RigidBody(Shape shape, Type type, glm::vec3 pos, glm::quat rot)
{
	this->shape = shape;
	this->type = type;
	physx::PxTransform transform(pos.x, pos.y, pos.z, physx::PxQuat(rot.x, rot.y, rot.z, rot.w));

	switch (type)
	{
	case Type::Dynamic:
		rigidDynamic = physx::PxCreateDynamic(*Physics::GetPhysicsObject(), transform, *shape.GetShape(), 1);
		Physics::AddActor(*rigidDynamic);
		break;
	case Type::Static:
		rigidStatic = physx::PxCreateStatic(*Physics::GetPhysicsObject(), transform, *shape.GetShape());
		Physics::AddActor(*rigidStatic);
		break;
	case Type::Kinematic:
		rigidDynamic = physx::PxCreateDynamic(*Physics::GetPhysicsObject(), transform, *shape.GetShape(), 1);
		Physics::AddActor(*rigidDynamic);
		rigidDynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		break;
	case Type::None:
		throw std::invalid_argument("Failed to create a rigidbody: invalid rigidbody type argument (RIGID_BODY_NONE)");
	}
}

RigidBody::~RigidBody()
{
	//Destroy();
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
	if (type == Type::None)
		return;
	physx::PxActor* actor = rigidDynamic == nullptr ? rigidStatic->is<physx::PxActor>() : rigidDynamic->is<physx::PxActor>();
	Physics::RemoveActor(*actor);
	type == Type::None;
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
	return physx::PxTransform(transform.position.x, transform.position.y, transform.position.z, physx::PxQuat(transform.rotation.x, transform.rotation.y, transform.rotation.z, transform.rotation.w));
}

physx::PxTransform RigidBody::GetTransform()
{
	return rigidStatic == nullptr ? rigidDynamic->getGlobalPose() : rigidStatic->getGlobalPose();
}