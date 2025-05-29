#pragma once
#include <string>
#include <string_view>

namespace sys // 'system' is an already defined symbol :(
{
	extern std::string GetEnvVariable(const std::string_view& name);

	extern bool StartProcess(const std::string_view& name, std::string args, bool waitForCompletion = true);

	extern bool OpenFile(const std::string_view& file); // returns false if file does not exist
}