#include "io/BinaryReader.h"
#include "io/BinaryWriter.h"

#include "io/FileFormat.h"
#include "io/FileMaterial.h"

uint64 FileImage::GetBinarySize()
{
	return SIZE_OF_NODE_HEADER + data.size() + sizeof(width) + sizeof(height);
}

void FileImage::Read(BinaryReader& reader)
{
	reader >> width >> height;

	data.resize(width * height * 4);
	reader >> data;
}

void FileImage::Write(BinaryWriter& writer)
{
	writer << width << height << data;
}

uint64 FileMaterial::GetBinarySize()
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
	albedo.Read(reader);

	reader >> type >> size;
	if (type != NODE_TYPE_NORMAL || size == 0)
		return; // could also throw
	normal.Read(reader);

	reader >> type >> size;
	if (type != NODE_TYPE_ROUGHNESS || size == 0)
		return; // could also throw
	roughness.Read(reader);

	reader >> type >> size;
	if (type != NODE_TYPE_METALLIC || size == 0)
		return; // could also throw
	metallic.Read(reader);

	reader >> type >> size;
	if (type != NODE_TYPE_AMBIENT_OCCLUSION || size == 0)
		return; // could also throw
	ambientOccl.Read(reader);
}

void FileMaterial::Write(BinaryWriter& writer)
{
	writer << NODE_TYPE_ALBEDO << albedo.GetBinarySize();
	albedo.Write(writer);

	writer << NODE_TYPE_NORMAL << normal.GetBinarySize();
	normal.Write(writer);

	writer << NODE_TYPE_ROUGHNESS << roughness.GetBinarySize();
	roughness.Write(writer);

	writer << NODE_TYPE_METALLIC << metallic.GetBinarySize();
	metallic.Write(writer);

	writer << NODE_TYPE_AMBIENT_OCCLUSION << ambientOccl.GetBinarySize();
	ambientOccl.Write(writer);
}