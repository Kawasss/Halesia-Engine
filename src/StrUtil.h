#pragma once
#include <string_view>
#include <optional>

namespace strutil
{
	extern std::optional<uint32_t> TryStringToUInt(const std::string_view& val);
}