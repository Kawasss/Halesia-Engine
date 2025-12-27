#include <charconv>

#include "StrUtil.h"

std::optional<uint32_t> strutil::TryStringToUInt(const std::string_view& val)
{
	if (val.empty())
		return std::optional<uint32_t>();

	const char* beg = &val[0];
	const char* end = beg + val.size();

	uint32_t ret = 0;
	std::from_chars_result res = std::from_chars(beg, end, ret);

	return res.ec == std::errc{} && res.ptr == end ? ret : std::optional<uint32_t>();
}