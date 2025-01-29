#pragma once
class BinaryReader;
class BinaryWriter;

using uint64 = unsigned long long;

struct FileBase
{
	virtual void Read(BinaryReader& reader)        = 0; // this will not read the node header
	virtual void Write(BinaryWriter& writer) const = 0; // this will write the node header
};