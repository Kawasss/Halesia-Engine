#include <Windows.h>

#include "system/System.h"

namespace sys
{
	std::string GetEnvVariable(const std::string_view& name)
	{
		constexpr uint32_t BUFFER_SIZE = 256;
		char buffer[BUFFER_SIZE]{};

		DWORD written = ::GetEnvironmentVariableA(name.data(), buffer, BUFFER_SIZE);

		if (written > BUFFER_SIZE)
			return ""; // the buffer is not big enough to store the variable

		std::string ret = buffer;
		return ret;
	}

	bool StartProcess(const std::string_view& name, std::string args, bool waitForCompletion)
	{
		STARTUPINFOA startInfo{};
		PROCESS_INFORMATION procInfo{};

		BOOL success = ::CreateProcessA(name.data(), args.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startInfo, &procInfo);
		if (!success)
			return false;

		if (waitForCompletion)
			::WaitForSingleObject(procInfo.hProcess, INFINITE);

		::CloseHandle(procInfo.hProcess);
		::CloseHandle(procInfo.hThread);
		return true;
	}
}