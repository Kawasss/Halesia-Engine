#include <cstdint>
#include <string>
#include <Windows.h>
#include <compressapi.h>

#include "io/CreationData.h"
#include "io/SceneWriter.h"
#include "io/BinaryWriter.h"
#include "io/DataArchiveFile.h"
#include "io/BinaryStream.h"

#include "core/Scene.h"
#include "core/Object.h"
#include "core/MeshObject.h"

#include "renderer/Texture.h"

static void WriteNamedReferencesToStream(BinaryStream& stream, const std::vector<Object*>& objects)
{
	uint32_t referenceCount = static_cast<uint32_t>(objects.size());
	stream << referenceCount;

	for (const Object* pObject : objects)
	{
		uint32_t strLen = static_cast<uint32_t>(pObject->name.size());

		stream << strLen;
		stream.Write(pObject->name.data(), strLen);
	}
}

static void WriteFullObjectToArchive(DataArchiveFile& archive, const Object* pObject)
{
	archive.AddData(pObject->name, pObject->Serialize());

	if (!pObject->HasChildren())
		return;

	BinaryStream childReferences;
	WriteNamedReferencesToStream(childReferences, pObject->GetChildren());

	archive.AddData(pObject->name + "_ref_children", childReferences.data);

	for (Object* pObject : pObject->GetChildren())
		WriteFullObjectToArchive(archive, pObject);
}

static void WriteTextureToStream(BinaryStream& stream, const Texture* pTexture)
{
	uint32_t width = 0, height = 0;

	width  = pTexture->GetWidth();
	height = pTexture->GetHeight();

	stream << width << height;

	if (width == 0 && height == 0)
		return;

	std::vector<char> data = pTexture->GetImageData();
	stream.Write(data.data(), data.size() * sizeof(char));
}

static void WriteMaterialsToArchive(DataArchiveFile& file)
{
	BinaryStream stream;
	uint32_t matCount = static_cast<uint32_t>(Mesh::materials.size());
	stream << matCount;

	for (uint32_t i = 1; i < matCount; i++)
	{
		std::string name = "##material" + std::to_string(i);
		uint32_t strLen = static_cast<uint32_t>(name.size());
		stream << strLen;

		stream.Write(name.data(), strLen);
	}

	file.AddData("##material_root", stream.data);	

	for (int i = 1; i < Mesh::materials.size(); i++)
	{
		stream.Clear();

		std::string name = "##material" + std::to_string(i);
		
		const Material& mat = Mesh::materials[i];

		WriteTextureToStream(stream, mat.albedo);
		WriteTextureToStream(stream, mat.normal);
		WriteTextureToStream(stream, mat.metallic);
		WriteTextureToStream(stream, mat.roughness);
		WriteTextureToStream(stream, mat.ambientOcclusion);

		file.AddData(name, stream.data);
	}
}

void HSFWriter::WriteSceneToArchive(const std::string& file, Scene* scene)
{
	DataArchiveFile archive(file, DataArchiveFile::OpenMethod::Clear);
	if (!archive.IsValid())
		return;

	BinaryStream rootReferences;
	WriteNamedReferencesToStream(rootReferences, scene->allObjects);
	archive.AddData("##object_root", rootReferences.data);

	for (Object* pObject : scene->allObjects)
		WriteFullObjectToArchive(archive, pObject);

	WriteMaterialsToArchive(archive);

	archive.WriteToFile();
}

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