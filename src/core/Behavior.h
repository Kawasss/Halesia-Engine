#pragma once
#include <set>
#include <string>

namespace Behavior // could also do something with converting the arguments into flags
{
	inline std::set<std::string> arguments;

	inline void ProcessArguments(int argc, char** argv)
	{
		for (int i = 0; i < argc; i++)
			arguments.insert(argv[i]);
	}
}