module;

#include <Windows.h>
#include <shellapi.h>
#include <Psapi.h>
#include <intrin.h>

module System;

import std;

namespace sys
{
	std::string GetEnvVariable(const std::string_view& name)
	{
		constexpr std::uint32_t BUFFER_SIZE = 256;
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

		::ShellExecuteA(NULL, NULL, file.data(), 0, 0, SW_HIDE);
		return true;
	}

	std::string GetProcessorName()
	{
		std::string ret;

		std::array<int, 4> integerBuffer = {};
		std::array<char, 64> charBuffer  = {};

		// https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=vs-2019
		constexpr std::array<int, 3> functionIds = {
			0x8000'0002, // manufacturer
			0x8000'0003, // model
			0x8000'0004  // clockspeed
		};

		for (int id : functionIds)
		{
			::__cpuid(integerBuffer.data(), id);
			std::memcpy(charBuffer.data(), integerBuffer.data(), sizeof(integerBuffer));

			ret += std::string(charBuffer.data());
		}

		return ret;
	}

	std::uint64_t GetPhysicalRAMCount()
	{
		ULONGLONG ret = 0;
		::GetPhysicallyInstalledSystemMemory(&ret);
		return ret * 1024;
	}
	

	std::uint64_t GetMemoryUsed()
	{
		PROCESS_MEMORY_COUNTERS counters{};
		bool res = ::GetProcessMemoryInfo(::GetCurrentProcess(), &counters, sizeof(counters));
		return res ? counters.WorkingSetSize : 0;
	}
}