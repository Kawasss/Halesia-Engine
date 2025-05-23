#include <cstdint>
#include <string>
#include <Windows.h>
#include <compressapi.h>

#include "io/CreationData.h"
#include "io/SceneWriter.h"
#include "io/BinaryWriter.h"

#include "core/Scene.h"
#include "core/Object.h"
#include "core/MeshObject.h"

void HSFWriter::WriteHSFScene(Scene* scene, std::string destination)
{
	BinaryWriter writer(destination);

	for (Object* object : scene->allObjects)
		WriteObject(writer, object);

	for (int i = 1; i < Mesh::materials.size(); i++)
		writer << FileMaterial::CreateFrom(Mesh::materials[i]);

	writer.WriteDataToFile(NODE_TYPE_COMPRESSION);
	writer.WriteDataToFile(sizeof(uint64_t) + sizeof(uint32_t));
	writer.WriteDataToFile(writer.GetCurrentSize());
	writer.WriteDataToFile(COMPRESS_ALGORITHM_XPRESS);

	writer.WriteToFileCompressed();
}

static void WriteRigidBody(BinaryWriter& writer, const RigidBody& rigid)
{
	writer << FileRigidBody::CreateFrom(rigid);
}

static void WriteTransform(BinaryWriter& writer, const Transform& transform)
{
	writer << NODE_TYPE_TRANSFORM;

	size_t pSize = writer.GetBase(); // location of the nodes size in the writers memory

	writer << 0ULL
	<< transform.position
	<< transform.rotation
	<< transform.scale;

	size_t end = writer.GetBase();
	size_t size = end - (pSize + sizeof(uint64_t));

	writer.SetBase(pSize);
	writer << size;
	writer.SetBase(end); // reset to the end of the writer
}

void HSFWriter::WriteObject(BinaryWriter& writer, Object* object)
{
	writer << NODE_TYPE_OBJECT;

	size_t pSize = writer.GetBase();

	writer 
		<< 0ULL
		<< object->state 
		<< NODE_TYPE_NAME
		<< object->name.size() + 1 
		<< object->name;

	WriteTransform(writer, object->transform);
	//WriteRigidBody(writer, object->rigid);

	//temp fix !
	FileMesh fileMesh = object->GetType() == Object::InheritType::Mesh ? FileMesh::CreateFrom(dynamic_cast<MeshObject*>(object)->mesh) : FileMesh::CreateFrom({});

	writer << fileMesh;
	size_t end = writer.GetBase();
	size_t size = end - (pSize + sizeof(uint64_t));

	writer.SetBase(pSize);
	writer << size;
	writer.SetBase(end); // reset to the end of the writer
}