module;

#include <Windows.h>
#include "core/Object.h"

#include "io/CreationData.h"
#include "io/BinaryStream.h"

#include "renderer/Vertex.h"

module Core.MeshObject;

import std;

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
	if (!data.hasMesh)
		return;

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

void MeshObject::SerializeSelf(BinaryStream& stream) const
{
	stream << mesh.uvScale;
	stream << mesh.cullBackFaces;
	stream << mesh.GetMaterialIndex();

	size_t vertexCount = mesh.vertices.size();
	stream << vertexCount;
	stream.Write(reinterpret_cast<const char*>(mesh.vertices.data()), vertexCount * sizeof(Vertex));

	size_t indexCount = mesh.indices.size();
	stream << indexCount;
	stream.Write(reinterpret_cast<const char*>(mesh.indices.data()), indexCount * sizeof(uint32_t));
}

void MeshObject::DeserializeSelf(const BinarySpan& stream)
{
	float uvScale = 1.0f;

	MeshCreationData creationData{};
	stream >> uvScale;
	stream >> creationData.cullBackFaces;
	stream >> creationData.materialIndex;

	size_t vertexCount = 0;
	stream >> vertexCount;

	creationData.vertices.resize(vertexCount);
	stream.Read(reinterpret_cast<char*>(creationData.vertices.data()), vertexCount * sizeof(Vertex));

	size_t indexCount = 0;
	stream >> indexCount;

	creationData.indices.resize(indexCount);
	stream.Read(reinterpret_cast<char*>(creationData.indices.data()), indexCount * sizeof(uint32_t));

	creationData.faceCount = creationData.indices.size() / 3;

	mesh.Create(creationData);
	mesh.uvScale = uvScale;
}

MeshObject::~MeshObject()
{
	mesh.Destroy();
}