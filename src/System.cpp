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
}