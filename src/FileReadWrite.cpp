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

void FileImage::Read(BinaryReader& reader)
{
	NodeType arrayType = NODE_TYPE_NONE;
	NodeSize arraySize = 0;

	reader >> width >> height >> arrayType >> arraySize;

	if (arrayType != NODE_TYPE_ARRAY)
		__debugbreak();

	reader >> data;
}

void FileMaterial::Read(BinaryReader& reader)
{
	NodeType type = NODE_TYPE_NONE;
	NodeType imageType = NODE_TYPE_NONE;
	NodeSize imageSize = 0;

	reader >> isLight;

	reader >> type;
	if (type != NODE_TYPE_ALBEDO)
		return; // could also throw
	reader >> imageType >> imageSize >> albedo;

	reader >> type;
	if (type != NODE_TYPE_NORMAL)
		return; // could also throw
	reader >> imageType >> imageSize >> normal;

	reader >> type;
	if (type != NODE_TYPE_ROUGHNESS)
		return; // could also throw
	reader >> imageType >> imageSize >> roughness;

	reader >> type;
	if (type != NODE_TYPE_METALLIC)
		return; // could also throw
	reader >> imageType >> imageSize >> metallic;

	reader >> type;
	if (type != NODE_TYPE_AMBIENT_OCCLUSION)
		return; // could also throw
	reader >> imageType >> imageSize >> ambientOccl;
}

void FileMesh::Read(BinaryReader& reader)
{
	NodeType arrayType = NODE_TYPE_NONE;
	NodeSize nSize = 0;

	reader >> materialIndex;
	reader >> arrayType;

	if (materialIndex == 2)
		materialIndex = 1; // debug

	if (arrayType != NODE_TYPE_VERTICES)
		__debugbreak();

	reader >> arrayType >> nSize;

	if (arrayType != NODE_TYPE_ARRAY)
		__debugbreak();

	reader >> vertices;
	reader >> arrayType;

	if (arrayType != NODE_TYPE_INDICES)
		__debugbreak();

	reader >> arrayType >> nSize;

	if (arrayType != NODE_TYPE_ARRAY)
		__debugbreak();

	reader >> indices;
}

void FileShape::Read(BinaryReader& reader)
{
	reader >> type >> x >> y >> z;
}

void FileRigidBody::Read(BinaryReader& reader)
{
	NodeType shapeType = NODE_TYPE_NONE;
	NodeSize shapeSize = 0;

	reader >> type >> shapeType >> shapeSize;

	if (shapeType != NODE_TYPE_SHAPE)
		__debugbreak();

	reader >> shape;
}
void FileImage::Write(BinaryWriter& writer) const
{
	writer << NODE_TYPE_IMAGE << 0ULL << width << height << data;
}

void FileMaterial::Write(BinaryWriter& writer) const
{
	writer
		<< NODE_TYPE_MATERIAL << 0ULL
		<< isLight
		<< NODE_TYPE_ALBEDO << albedo
		<< NODE_TYPE_NORMAL << normal
		<< NODE_TYPE_ROUGHNESS << roughness
		<< NODE_TYPE_METALLIC << metallic
		<< NODE_TYPE_AMBIENT_OCCLUSION << ambientOccl;
}

void FileMesh::Write(BinaryWriter& writer) const
{
	writer << NODE_TYPE_MESH << 0ULL << materialIndex << NODE_TYPE_VERTICES << vertices << NODE_TYPE_INDICES << indices;
}

void FileShape::Write(BinaryWriter& writer) const
{
	writer << NODE_TYPE_SHAPE << 0ULL << type << x << y << z;
}

void FileRigidBody::Write(BinaryWriter& writer) const
{
	writer << NODE_TYPE_RIGIDBODY << 0ULL << type << shape;
}

FileImage FileImage::CreateFrom(Texture* tex)
{
	FileImage ret;

	ret.width  = tex->GetWidth();
	ret.height = tex->GetHeight();
	ret.data   = FileArray<char>::CreateFrom(tex->GetImageData());

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
	ret.indices = FileArray<uint32_t>::CreateFrom(mesh.indices);
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