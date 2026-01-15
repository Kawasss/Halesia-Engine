#pragma once
#include <vector>
#include <string>
#include "FileFormat.h"

class Scene;
class Object;
class BinaryWriter;

namespace HSFWriter
{
	extern void WriteSceneToArchive(const std::string& file, const Scene* scene);
}