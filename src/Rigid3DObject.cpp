module;

#include <Windows.h>

#include "io/CreationData.h"

module Core.Rigid3DObject;

import Physics.RigidBody;
import Physics.Shapes;

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
	rigid.Init(Shape::GetShapeFromType(data.hitBox.shapeType, data.hitBox.extents), data.hitBox.rigidType);
	rigid.SetUserData(this);
}

void Rigid3DObject::DuplicateDataTo(Object* pObject) const
{
	Rigid3DObject* pRigid = dynamic_cast<Rigid3DObject*>(pObject);
	pRigid->rigid = RigidBody(rigid.shape, rigid.type);
}