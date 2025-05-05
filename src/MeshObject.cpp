#include <Windows.h>
#include "core/MeshObject.h"

#include "io/CreationData.h"

MeshObject::MeshObject() : Object(InheritType::Mesh)
{

}

MeshObject* MeshObject::Create(const ObjectCreationData& data)
{
	MeshObject* ret = new MeshObject();

	ret->Initialize(data);
	ret->Init(data);

	return ret;
}

MeshObject* MeshObject::Create()
{
	return new MeshObject();
}

void MeshObject::Init(const ObjectCreationData& data)
{
	if (data.hasMesh)
		mesh.Create(data.mesh);
}

bool MeshObject::MeshIsValid() const
{
	return mesh.IsValid();
}

void MeshObject::DuplicateDataTo(Object* pObject) const
{
	MeshObject* pMesh = dynamic_cast<MeshObject*>(pObject);
	pMesh->mesh.CopyFrom(mesh);
}

MeshObject::~MeshObject()
{
	mesh.Destroy();
}