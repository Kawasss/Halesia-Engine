#pragma once
#include <string>

namespace sys // 'system' is an already defined symbol :(
{
	extern std::string GetEnvVariable(const std::string_view& name);

	extern bool StartProcess(const std::string_view& name, std::string args, bool waitForCompletion = true);
}