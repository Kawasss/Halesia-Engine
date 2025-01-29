#pragma once
#include "FileBase.h"
#include "FileShape.h"

class RigidBody;

using byte = unsigned char;

struct FileRigidBody : FileBase
{
	enum class Type : byte
	{
		None      = 0,
		Static    = 1,
		Dynamic   = 2,
		Kinematic = 3,
	};

	Type type = Type::None;
	FileShape shape;

	static FileRigidBody CreateFrom(const RigidBody& rigid);

	void Write(BinaryWriter& writer) const override;
	void Read(BinaryReader& reader) override;
};