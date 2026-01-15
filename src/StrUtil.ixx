export module StrUtil;

import std;

namespace strutil
{
	template<typename T>
	concept IsPrimitive = std::is_fundamental_v<T>;

	export template<IsPrimitive T>
	extern std::optional<T> TryStringTo(const std::string_view& val)
	{
		if (val.empty())
			return std::optional<T>();

		const char* beg = &val[0];
		const char* end = beg + val.size();

		T ret{};
		std::from_chars_result res = std::from_chars(beg, end, ret);

		return res.ec == std::errc{} && res.ptr == end ? ret : std::optional<T>();
	}
}