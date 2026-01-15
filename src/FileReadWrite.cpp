#include "io/BinaryReader.h"
#include "io/BinaryWriter.h"

#include "io/FileFormat.h"
#include "io/FileMaterial.h"
#include "io/FileArray.h"

#include "renderer/Texture.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"

import Physics.RigidBody;

void FileImage::Read(BinaryReader& reader)
{
	NodeType arrayType = NODE_TYPE_NONE;
	NodeSize arraySize = 0;

	reader >> arrayType >> arraySize;

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

void FileImage::Write(BinaryWriter& writer) const
{
	writer << NODE_TYPE_IMAGE << 0ULL << data;
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

FileImage FileImage::CreateFrom(Texture* tex)
{
	FileImage ret;
	ret.data = FileArray<char>::CreateFrom(tex->GetImageData());

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