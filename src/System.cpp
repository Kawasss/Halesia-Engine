#include <Windows.h>
#include <shellapi.h>
#include <filesystem>

#include "system/System.h"

namespace sys
{
	std::string GetEnvVariable(const std::string_view& name)
	{
		constexpr uint32_t BUFFER_SIZE = 256;
		char buffer[BUFFER_SIZE]{};

		DWORD written = ::GetEnvironmentVariableA(name.data(), buffer, BUFFER_SIZE);

		if (written == 0)
			return "";

		std::string ret;

		if (written > BUFFER_SIZE)
		{
			ret.resize(written);
			::GetEnvironmentVariableA(name.data(), ret.data(), written);
		}
		else
		{
			ret = buffer;
		}

		return ret;
	}

	bool StartProcess(const std::string_view& name, std::string args, bool waitForCompletion)
	{
		STARTUPINFOA startInfo{};
		PROCESS_INFORMATION procInfo{};

		BOOL success = ::CreateProcessA(name.data(), args.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startInfo, &procInfo);
		if (!success)
			return false;

		DWORD exitCode = 0;

		if (waitForCompletion)
		{
			::WaitForSingleObject(procInfo.hProcess, INFINITE);
			::GetExitCodeProcess(procInfo.hProcess, &exitCode);
		}

		::CloseHandle(procInfo.hProcess);
		::CloseHandle(procInfo.hThread);
		return exitCode == 0;
	}

	bool OpenFile(const std::string_view& file)
	{
		if (!std::filesystem::exists(file))
			return false;

		ShellExecuteA(NULL, NULL, file.data(), 0, 0, SW_HIDE);
		return true;
	}
}