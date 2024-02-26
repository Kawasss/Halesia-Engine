#pragma once
#include <vector>
#include <string>
#include "FileFormat.h"

class Scene;
class Object;

namespace HSFWriter
{
	inline extern void WriteHSFScene(Scene* scene, std::string destination);
	inline extern void WriteObject(Object* object, std::string destination);
}