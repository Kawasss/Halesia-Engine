#pragma once
#include <vector>
#include <string>

class Scene;

namespace HSFWriter
{
	inline extern void WriteHSFScene(Scene* scene, std::string destination);
}