#include <Windows.h>
#include "core/MeshObject.h"

#include "io/CreationData.h"

MeshObject::MeshObject() : Object(InheritType::Mesh)
{

}

MeshObject* MeshObject::Create(const ObjectCreationData& data)
{
	assert(data.hasMesh);

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
	mesh.Create(data.mesh);
}

bool MeshObject::MeshIsValid() const
{
	return mesh.IsValid();
}

MeshObject::~MeshObject()
{
	mesh.Destroy();
}