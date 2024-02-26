#pragma once
#include <vector>
#include <string>
#include "FileFormat.h"

class Scene;
class Object;
class BinaryWriter;

namespace HSFWriter
{
	inline extern void WriteHSFScene(Scene* scene, std::string destination);
	inline extern void WriteObject(BinaryWriter& writer, Object* object);
}