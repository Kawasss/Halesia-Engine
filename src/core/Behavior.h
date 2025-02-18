#pragma once
#include <vector>
#include <string>

namespace Behavior // could also do something with converting the arguments into flags
{
	inline std::vector<std::string_view> arguments;

	inline void ProcessArguments(int argc, char** argv)
	{
		for (int i = 0; i < argc; i++)
			arguments.push_back(argv[i]);
	}
}