#pragma once
class BinaryReader;
class BinaryWriter;

using uint64 = unsigned long long;

struct FileBase
{
	virtual uint64 GetBinarySize() const = 0; // returns the size WITH the size of the node header

	virtual void Read(BinaryReader& reader)        = 0;
	virtual void Write(BinaryWriter& writer) const = 0; // this will write the node header
};