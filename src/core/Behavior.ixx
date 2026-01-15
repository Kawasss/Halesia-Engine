export module Core.Behavior;

import std;

namespace Behavior
{
	export std::vector<std::string_view> arguments;

	export void ProcessArguments(int argc, char** argv)
	{
		arguments.reserve(argc);
		for (int i = 0; i < argc; i++)
			arguments.push_back(argv[i]);
	}
}