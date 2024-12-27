#include "io/BinaryReader.h"
#include "io/BinaryWriter.h"

#include "io/FileFormat.h"
#include "io/FileMaterial.h"
#include "io/FileArray.h"
#include "io/FileMesh.h"
#include "io/FileShape.h"
#include "io/FileRigidBody.h"

#include "renderer/Texture.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"

#include "physics/RigidBody.h"

uint64 FileImage::GetBinarySize() const
{
	return data.size() + sizeof(width) + sizeof(height);
}

uint64 FileMaterial::GetBinarySize() const
{   // account for the node headers of the textures
	return SIZE_OF_NODE_HEADER * 5 + albedo.GetBinarySize() + normal.GetBinarySize() + roughness.GetBinarySize() + metallic.GetBinarySize() + ambientOccl.GetBinarySize() + sizeof(isLight);
}

uint64 FileMesh::GetBinarySize() const
{
	// account for the headers of the vertices and indices node
	return SIZE_OF_NODE_HEADER * 2 + vertices.GetBinarySize() + indices.GetBinarySize() + sizeof(materialIndex);
}

void FileImage::Read(BinaryReader& reader)
{
	reader >> width >> height;

	data.resize(width * height * 4);
	reader >> data;
}

void FileMaterial::Read(BinaryReader& reader)
{
	NodeType type = NODE_TYPE_NONE;
	uint64 size = 0;

	reader >> isLight;

	reader >> type >> size;
	if (type != NODE_TYPE_ALBEDO || size == 0)
		return; // could also throw
	reader >> albedo;

	reader >> type >> size;
	if (type != NODE_TYPE_NORMAL || size == 0)
		return; // could also throw
	reader >> normal;

	reader >> type >> size;
	if (type != NODE_TYPE_ROUGHNESS || size == 0)
		return; // could also throw
	reader >> roughness;

	reader >> type >> size;
	if (type != NODE_TYPE_METALLIC || size == 0)
		return; // could also throw
	reader >> metallic;

	reader >> type >> size;
	if (type != NODE_TYPE_AMBIENT_OCCLUSION || size == 0)
		return; // could also throw
	reader >> ambientOccl;
}

void FileMesh::Read(BinaryReader& reader)
{
	NodeType type;
	reader >> materialIndex >> type >> vertices >> type >> indices;
}

void FileShape::Read(BinaryReader& reader)
{
	reader >> type >> x >> y >> z;
}

void FileRigidBody::Read(BinaryReader& reader)
{
	reader >> type >> shape;
}

void FileImage::Write(BinaryWriter& writer) const // file images are special in that they dont write their node type
{
	writer << GetBinarySize() << width << height << data;
}

void FileMaterial::Write(BinaryWriter& writer) const
{
	writer 
		<< NODE_TYPE_ALBEDO            << albedo
		<< NODE_TYPE_NORMAL            << normal
		<< NODE_TYPE_ROUGHNESS         << roughness
		<< NODE_TYPE_METALLIC          << metallic
		<< NODE_TYPE_AMBIENT_OCCLUSION << ambientOccl;
}

void FileMesh::Write(BinaryWriter& writer) const
{
	writer << NODE_TYPE_MESH << GetBinarySize() << materialIndex << NODE_TYPE_VERTICES << vertices << NODE_TYPE_INDICES << indices;
}

void FileShape::Write(BinaryWriter& writer) const // will not write its node type and size
{
	writer << type << x << y << z;
}

void FileRigidBody::Write(BinaryWriter& writer) const
{
	writer << NODE_TYPE_RIGIDBODY << GetBinarySize() << type << shape;
}

FileImage FileImage::CreateFrom(Texture* tex)
{
	FileImage ret;

	ret.width  = tex->GetWidth();
	ret.height = tex->GetHeight();
	ret.data   = tex->GetImageData();

	return ret;
}

FileMaterial FileMaterial::CreateFrom(const Material& mat)
{
	FileMaterial ret;

	ret.isLight     = mat.isLight;
	ret.albedo      = FileImage::CreateFrom(mat.albedo);
	ret.normal      = FileImage::CreateFrom(mat.normal);
	ret.roughness   = FileImage::CreateFrom(mat.roughness);
	ret.metallic    = FileImage::CreateFrom(mat.metallic);
	ret.ambientOccl = FileImage::CreateFrom(mat.ambientOcclusion);

	return ret;
}

FileMesh FileMesh::CreateFrom(const Mesh& mesh)
{
	FileMesh ret;

	ret.vertices = FileArray<Vertex>::CreateFrom(mesh.vertices);
	ret.indices = FileArray<uint16_t>::CreateFrom(mesh.indices);
	ret.materialIndex = mesh.GetMaterialIndex();

	return ret;
}

FileRigidBody FileRigidBody::CreateFrom(const RigidBody& rigid)
{
	FileRigidBody ret;

	ret.type       = static_cast<FileRigidBody::Type>(rigid.type);
	ret.shape.type = static_cast<FileShape::Type>(rigid.shape.type);
	ret.shape.x    = rigid.shape.data.x;
	ret.shape.y    = rigid.shape.data.y;
	ret.shape.z    = rigid.shape.data.z;

	return ret;
}