#include "io/BinaryReader.h"
#include "io/BinaryWriter.h"

#include "io/FileFormat.h"
#include "io/FileMaterial.h"

uint64 FileImage::GetBinarySize() const
{
	return SIZE_OF_NODE_HEADER + data.size() + sizeof(width) + sizeof(height);
}

void FileImage::Read(BinaryReader& reader)
{
	reader >> width >> height;

	data.resize(width * height * 4);
	reader >> data;
}

void FileImage::Write(BinaryWriter& writer) const
{
	writer << width << height << data;
}

uint64 FileMaterial::GetBinarySize() const
{   // account for the node headers of the textures
	return SIZE_OF_NODE_HEADER + albedo.GetBinarySize() + normal.GetBinarySize() + roughness.GetBinarySize() + metallic.GetBinarySize() + ambientOccl.GetBinarySize() + sizeof(isLight);
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

void FileMaterial::Write(BinaryWriter& writer) const
{
	writer 
		<< NODE_TYPE_ALBEDO            << albedo.GetBinarySize()      << albedo
		<< NODE_TYPE_NORMAL            << normal.GetBinarySize()      << normal
		<< NODE_TYPE_ROUGHNESS         << roughness.GetBinarySize()   << roughness
		<< NODE_TYPE_METALLIC          << metallic.GetBinarySize()    << metallic
		<< NODE_TYPE_AMBIENT_OCCLUSION << ambientOccl.GetBinarySize() << ambientOccl;
}