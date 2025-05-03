#include "core/Rigid3DObject.h"

#include "io/CreationData.h"

Rigid3DObject::Rigid3DObject() : Object(InheritType::Rigid3D)
{

}

Rigid3DObject::~Rigid3DObject()
{
	rigid.Destroy();
}

Rigid3DObject* Rigid3DObject::Create(const ObjectCreationData& data)
{
	Rigid3DObject* ret = new Rigid3DObject();
	ret->Init(data);

	return ret;
}

Rigid3DObject* Rigid3DObject::Create()
{
	return new Rigid3DObject();
}

void Rigid3DObject::Init(const ObjectCreationData& data)
{
	Initialize(data);
	new(&rigid) RigidBody(Shape::GetShapeFromType(data.hitBox.shapeType, data.hitBox.extents), data.hitBox.rigidType);
}