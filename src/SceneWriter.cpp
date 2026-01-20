module;

#include <Windows.h>

#include "io/BinaryStream.h"

#include "core/Object.h"

#include "renderer/Texture.h"
#include "renderer/Mesh.h"

module IO.SceneWriter;

import std;

import Core.Scene;
import Core.MeshObject;

import IO.DataArchiveFile;

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
	std::vector<char> data = pTexture->GetAsInternalFormat();
	stream << data.size();

	stream.Write(data.data(), data.size() * sizeof(char));
}

static void WriteMaterialsToArchive(DataArchiveFile& file)
{
	BinaryStream stream;
	uint32_t matCount = static_cast<uint32_t>(Mesh::materials.size() - 1);
	stream << matCount;

	for (uint32_t i = 1; i < matCount + 1; i++)
	{
		std::string name = "##material" + std::to_string(i);
		uint32_t strLen = static_cast<uint32_t>(name.size());
		stream << strLen;

		stream.Write(name.data(), strLen);
	}

	file.AddData("##material_root", stream.data);	

	std::vector<int> indices(matCount);
	for (int i = 0; i < indices.size(); i++)
		indices[i] = i + 1;

	win32::CriticalSection critSection;

	std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
		[&](int i)
		{
			BinaryStream matStream;

			std::string name = "##material" + std::to_string(i);
			const Material& mat = Mesh::materials[i];

			WriteTextureToStream(matStream, mat.albedo);
			WriteTextureToStream(matStream, mat.normal);
			WriteTextureToStream(matStream, mat.metallic);
			WriteTextureToStream(matStream, mat.roughness);
			WriteTextureToStream(matStream, mat.ambientOcclusion);

			win32::CriticalLockGuard lockGuard(critSection);
			file.AddData(name, matStream.data);
		}
	);
}

void SceneWriter::WriteSceneToArchive(const std::string& file, const Scene* scene)
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